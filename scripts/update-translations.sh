#!/usr/bin/env bash
# Refresh the window's translation files, src/app/translations/, from the
# words in its source (docs/design.md § Wording and languages).
#
# Usage:
#   scripts/update-translations.sh              refresh the files in place
#   scripts/update-translations.sh --check DIR  change nothing; fail if a file
#                                               is out of date, or English
#                                               lacks a plural form. DIR is
#                                               scratch. CTest runs this.
# Through CMake: cmake --build <build folder> --target update-translations
#
# LUPDATE names Qt 6's lupdate; without it, lupdate6 or lupdate on PATH.
#
# English is the language the source is written in, so its file holds only
# the plural forms. Every other language's file holds every sentence. Files
# carry no line numbers, so moving code does not change them.
set -Eeuo pipefail

mode=update
work=
case "$#:${1:-}" in
    0:) ;;
    # Made absolute before the cd below, so a relative DIR is the caller's.
    2:--check) mode=check; work=$(realpath -m -- "$2") ;;
    *) sed -n '/^# Usage:/,/^# Through/{s/^# \{0,1\}//;p}' "$0" >&2
       exit 2 ;;
esac
cd "$(dirname "$0")/.."

lupdate=${LUPDATE:-$(command -v lupdate6 || command -v lupdate || true)}
if [[ -z $lupdate ]]; then
    echo "update-translations: lupdate not found; set LUPDATE to Qt 6's lupdate." >&2
    exit 2
fi

src=src/app
status=0
# With none, the pattern itself would reach lupdate, which makes the file.
shopt -s nullglob
files=("$src"/translations/defragr_*.ts)
if (( ${#files[@]} == 0 )); then
    echo "update-translations: no translation files in $src/translations." >&2
    exit 2
fi
for ts in "${files[@]}"; do
    target=$ts
    if [[ $mode == check ]]; then
        mkdir -p "$work"
        target=$work/${ts##*/}
        cp "$ts" "$target"
    fi
    options=(-silent -locations none -no-obsolete -extensions 'cpp,h')
    if [[ $ts == */defragr_en.ts ]]; then
        options+=(-pluralonly)
    fi
    "$lupdate" "${options[@]}" "$src" -ts "$target"

    # Without its forms, a plural sentence shows as "file(s)".
    if [[ $ts == */defragr_en.ts ]] && grep -q 'type="unfinished"' "$target"; then
        echo "update-translations: $ts has a plural sentence without its English forms." >&2
        echo "Fill in each <numerusform> marked unfinished, then remove the mark." >&2
        status=1
    fi
    if [[ $mode == check ]] && ! cmp -s "$ts" "$target"; then
        echo "update-translations: $ts is out of date. Run scripts/update-translations.sh." >&2
        diff -u "$ts" "$target" >&2 || true
        status=1
    fi
done
exit "$status"
