#!/usr/bin/env bash
# Check that every file carrying DeFragR's version agrees with
# CMakeLists.txt: the README and the OBS packaging in packaging/obs/.
# The release recipe, .claude/bump.json, runs it after a bump.
#
# Usage: scripts/check-version-drift.sh
set -Eeuo pipefail
cd "$(dirname "$0")/.."

# project(DeFragR spans lines; its VERSION line is the first line that is
# nothing but VERSION and a version.
version=$(sed -nE '/^ +VERSION [0-9]+\.[0-9]+\.[0-9]+$/{s/^ +VERSION //p;q}' CMakeLists.txt)
if [[ -z $version ]]; then
    echo "check-version-drift: no VERSION line in CMakeLists.txt" >&2
    exit 2
fi

status=0
fail() {
    echo "check-version-drift: $1" >&2
    status=1
}
# FILE holds TEXT somewhere.
has_text() { grep -qF -- "$2" "$1" || fail "$1 lacks: $2"; }
# FILE has a line that is exactly TEXT.
has_line() { grep -qxF -- "$2" "$1" || fail "$1 lacks the line: $2"; }

obs=packaging/obs
has_text README.md "Version $version. [CHANGELOG.md]"
has_text "$obs/_service" "roll defragr-$version.tar.gz,"
has_text "$obs/_service" "<param name=\"revision\">v$version</param>"
has_text "$obs/_service" "<param name=\"versionformat\">$version</param>"
has_line "$obs/defragr.spec" "Version:        $version"
has_line "$obs/PKGBUILD" "pkgver=$version"
# A new version starts again at the first package release.
has_line "$obs/PKGBUILD" "pkgrel=1"
has_line "$obs/defragr.dsc" "Version: $version-1"
has_line "$obs/defragr.dsc" "DEBTRANSFORM-TAR: defragr-$version.tar.gz"

# The newest entry of each package changelog is this version's.
newest=$(head -n 1 "$obs/debian.changelog")
[[ $newest == "defragr ($version-1) "* ]] ||
    fail "$obs/debian.changelog's newest entry is not $version-1: $newest"
newest=$(grep -m 1 '^- ' "$obs/defragr.changes" || true)
[[ $newest == "- Version $version:"* ]] ||
    fail "$obs/defragr.changes's newest entry is not $version: $newest"

if ((status == 0)); then
    echo "check-version-drift: every file is at $version."
fi
exit "$status"
