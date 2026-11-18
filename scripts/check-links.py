#!/usr/bin/env python3
"""Fail when a relative Markdown link in a tracked file points at nothing.

Checks only that the target file or folder exists; it does not check web
links or #anchors. Inline links, wrapped across lines or not, and reference
definitions are checked; a target in <angle brackets> or with %-escapes is
read as Markdown reads it. Links inside fenced code blocks and inline code
are skipped.
"""
import pathlib
import re
import subprocess
import sys
import urllib.parse

TARGET = r"(<[^<>\n]*>|[^)\s]+)"
# A link's text may wrap; its target and title may sit on the next line.
LINK = re.compile(r"\[[^\]]*\]\(\s*" + TARGET + r"(?:\s+\"[^\"]*\")?\s*\)")
DEFINITION = re.compile(r"^ {0,3}\[[^\]]+\]:\s*(<[^<>\n]*>|\S+)", re.M)
INLINE_CODE = re.compile(r"`[^`]*`")


FENCE = re.compile(r"(`{3,}|~{3,})(.*)")


def tracked_markdown():
    # -z: without it git quotes a name with non-ASCII bytes.
    out = subprocess.run(["git", "ls-files", "-z", "*.md"], check=True,
                         capture_output=True, text=True).stdout
    return [pathlib.Path(p) for p in out.split("\0") if p]


def paragraphs(path):
    """Each run of lines outside code blocks, ended by a blank line, as
    (its first line's number, its text)."""
    fence = None  # the marker that opened the current code block
    start, lines = 0, []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        marker = FENCE.match(line.lstrip())
        if fence is None and marker:
            fence = marker.group(1)
        elif fence is not None:
            # Only a run of the same character, at least as long, with
            # nothing after it, closes the block.
            if (marker and marker.group(1)[0] == fence[0]
                    and len(marker.group(1)) >= len(fence)
                    and not marker.group(2).strip()):
                fence = None
            continue
        if fence is not None or not line.strip():
            if lines:
                yield start, "\n".join(lines)
            start, lines = 0, []
            continue
        if not lines:
            start = number
        lines.append(line)
    if lines:
        yield start, "\n".join(lines)


def broken_links(path):
    for first, text in paragraphs(path):
        text = INLINE_CODE.sub(lambda code: "\n" * code.group(0).count("\n"), text)
        found = [(m.start(), m.group(1)) for m in LINK.finditer(text)]
        found += [(m.start(), m.group(1)) for m in DEFINITION.finditer(text)]
        for offset, target in found:
            if target.startswith("<") and target.endswith(">"):
                target = target[1:-1]
            if re.match(r"^[a-z][a-z0-9+.-]*:", target, re.I) or target.startswith("#"):
                continue
            file_part = urllib.parse.unquote(target.split("#", 1)[0])
            # A link starting with / is read from the repository root, as
            # GitHub does; the gate runs this from the root.
            if file_part.startswith("/"):
                resolved = pathlib.Path(file_part.lstrip("/") or ".")
            else:
                resolved = path.parent / file_part
            if not resolved.exists():
                yield first + text.count("\n", 0, offset), target


def main():
    failures = 0
    for path in tracked_markdown():
        for number, target in broken_links(path):
            print(f"{path}:{number}: link target does not exist: {target}")
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
