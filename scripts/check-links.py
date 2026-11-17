#!/usr/bin/env python3
"""Fail when a relative Markdown link in a tracked file points at nothing.

Checks only that the target file or folder exists; it does not check web
links or #anchors. Links inside fenced code blocks and inline code are
skipped.
"""
import pathlib
import re
import subprocess
import sys

LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
INLINE_CODE = re.compile(r"`[^`]*`")


FENCE = re.compile(r"(`{3,}|~{3,})(.*)")


def tracked_markdown():
    # -z: without it git quotes a name with non-ASCII bytes.
    out = subprocess.run(["git", "ls-files", "-z", "*.md"], check=True,
                         capture_output=True, text=True).stdout
    return [pathlib.Path(p) for p in out.split("\0") if p]


def broken_links(path):
    fence = None  # the marker that opened the current code block
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        marker = FENCE.match(line.lstrip())
        if fence is None and marker:
            fence = marker.group(1)
            continue
        if fence is not None:
            # Only a run of the same character, at least as long, with
            # nothing after it, closes the block.
            if (marker and marker.group(1)[0] == fence[0]
                    and len(marker.group(1)) >= len(fence)
                    and not marker.group(2).strip()):
                fence = None
            continue
        for target in LINK.findall(INLINE_CODE.sub("", line)):
            if re.match(r"^[a-z][a-z0-9+.-]*:", target, re.I) or target.startswith("#"):
                continue
            file_part = target.split("#", 1)[0]
            # A link starting with / is read from the repository root, as
            # GitHub does; the gate runs this from the root.
            if file_part.startswith("/"):
                resolved = pathlib.Path(file_part.lstrip("/") or ".")
            else:
                resolved = path.parent / file_part
            if not resolved.exists():
                yield number, target


def main():
    failures = 0
    for path in tracked_markdown():
        for number, target in broken_links(path):
            print(f"{path}:{number}: link target does not exist: {target}")
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
