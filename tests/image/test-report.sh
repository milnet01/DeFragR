#!/usr/bin/env bash
# The image_report test: the engine's report on a fragmented image, run
# in the test machine by check-report.sh, then decoded here by
# report-check, which compares it with filefrag's reference.
# Usage: test-report.sh WORKDIR ENGINE REPORT_CHECK HOLD_LEASE
set -Eeuo pipefail

here=$(cd "$(dirname "$0")" && pwd)
usage="usage: test-report.sh WORKDIR ENGINE REPORT_CHECK HOLD_LEASE"
work=${1:?$usage} engine=${2:?$usage} checker=${3:?$usage} lease=${4:?$usage}
mkdir -p "$work"
rm -f "$work/img.img"
truncate -s 256M "$work/img.img"

rc=0
"$here/../vm/run.sh" --work "$work/machine" \
    --copy "$here/fragment.sh" --copy "$here/snap.squashfs" --copy "$engine" --copy "$lease" \
    --disk "img=$work/img.img" "$here/check-report.sh" || rc=$?
(( rc == 0 )) || exit "$rc"

# section NAME prints the lines check-report.sh put between its markers.
section() {
    sed -n "/^DFR-$1-BEGIN\$/,/^DFR-$1-END\$/{//!p}" "$work/machine/console.log"
}
section REFERENCE >"$work/reference"
section BLOCKS >"$work/blocks"
section REPORT | base64 -d >"$work/report.bin"
section SUBDIR | base64 -d >"$work/subdir.bin"
section INODES >"$work/inodes"
section SPACE >"$work/space"
section EXTENTS >"$work/extents"

exec "$checker" "$work/reference" "$work/blocks" "$work/report.bin" "$work/subdir.bin" \
    "$work/inodes" "$work/space" "$work/extents"
