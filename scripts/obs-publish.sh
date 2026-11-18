#!/usr/bin/env bash
# Upload packaging/obs/ to the openSUSE Build Service package
# home:milnet:DeFragR/defragr, then wait until every repository has built
# this version. Run it after the release tag is pushed: the package's
# _service fetches that tag from GitHub.
#
# Usage:
#   scripts/obs-publish.sh           upload, then wait
#   scripts/obs-publish.sh --check   change nothing; fail if osc cannot read
#                                    the package. The release recipe runs
#                                    this before it bumps anything.
#
# Needs python3, and osc already signed in. The package is checked out under
# OBS_WORKDIR, build/obs by default. Waiting can take an hour or more.
set -Eeuo pipefail

project=home:milnet:DeFragR
package=defragr
mode=publish
case "$#:${1:-}" in
    0:) ;;
    1:--check) mode=check ;;
    *) sed -n '/^# Usage:/,/^#$/{s/^# \{0,1\}//;p}' "$0" >&2
       exit 2 ;;
esac
cd "$(dirname "$0")/.."

die() {
    echo "obs-publish: $1" >&2
    exit 1
}

for tool in osc python3; do
    command -v "$tool" >/dev/null || die "$tool not found; install it."
done
# No stdin, so a password prompt fails instead of waiting.
osc meta pkg "$project" "$package" </dev/null >/dev/null ||
    die "osc cannot read $project/$package; is it signed in?"
[[ $mode == check ]] && exit 0

bash scripts/check-version-drift.sh
version=$(sed -n 's/^pkgver=//p' packaging/obs/PKGBUILD)
git ls-remote --exit-code --tags origin "refs/tags/v$version" >/dev/null ||
    die "tag v$version is not on GitHub; push it first."

work=${OBS_WORKDIR:-build/obs}
checkout=$work/$project/$package
if [[ -d $checkout/.osc ]]; then
    (cd "$checkout" && osc update)
else
    mkdir -p "$work"
    (cd "$work" && osc checkout "$project" "$package")
fi
# Files only on OBS, such as the NASM source tarball, stay as they are.
cp -p packaging/obs/* "$checkout/"
(
    cd "$checkout"
    osc addremove
    if [[ -n $(osc status) ]]; then
        osc commit -m "Update to $version"
    else
        echo "obs-publish: OBS already has these files."
    fi
)

# Results read just after an upload can still be the last version's, so a
# repository is done only once its built packages carry this version. An
# upload that keeps the version, such as a packaging fix, is done at once.
# One request returns every repository's status and built files; this
# prints a line per repository: name, status, and "new" when a built file
# is this version's.
read_results='
import re, sys, xml.etree.ElementTree as tree
package, version = sys.argv[1:]
built = re.compile(re.escape(package) + "[-_]" + re.escape(version) + "-")
for result in tree.parse(sys.stdin).getroot().iter("result"):
    status = result.find("status")
    files = [b.get("filename", "") for b in result.iter("binary")]
    print(result.get("repository"), status.get("code") if status is not None else "none",
          "new" if any(built.match(f) for f in files) else "old")
'
deadline=$((SECONDS + 4 * 3600))
while :; do
    # Read first, so a failed or empty answer stops here rather than
    # reading as nothing left to wait for.
    results=$(osc api "/build/$project/_result?package=$package&view=binarylist&view=status" |
                  python3 -I -c "$read_results" "$package" "$version")
    [[ -n $results ]] || die "OBS lists no repositories for $project/$package."
    waiting=()
    while read -r repo status built; do
        case $status in
            excluded|disabled) continue ;;
            failed|unresolvable|broken) die "$repo: $status." ;;
        esac
        [[ $status == succeeded && $built == new ]] || waiting+=("$repo")
    done <<<"$results"
    if ((${#waiting[@]} == 0)); then
        echo "obs-publish: every repository has built $version."
        exit 0
    fi
    ((SECONDS < deadline)) || die "still building after four hours: ${waiting[*]}"
    echo "obs-publish: waiting for ${waiting[*]}"
    sleep 60
done
