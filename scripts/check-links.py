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


def tracked_markdown():
    out = subprocess.run(["git", "ls-files", "*.md"], check=True,
                         capture_output=True, text=True).stdout
    return [pathlib.Path(p) for p in out.splitlines()]


def broken_links(path):
    fenced = False
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line.lstrip().startswith(("```", "~~~")):
            fenced = not fenced
            continue
        if fenced:
            continue
        for target in LINK.findall(INLINE_CODE.sub("", line)):
            if re.match(r"^[a-z][a-z0-9+.-]*:", target, re.I) or target.startswith("#"):
                continue
            file_part = target.split("#", 1)[0]
            if not (path.parent / file_part).exists():
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
