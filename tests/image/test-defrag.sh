#!/usr/bin/env bash
# The image_defrag test. In one test machine, check-defrag.sh defrags a
# fragmented image; defrag-check then compares the engine's records with
# filefrag's view before and after, and the map the first defrag sent
# with the report taken just before it. In a second, whose CPU has no SSE4.2,
# check-old-cpu.sh checks defrag is refused there and report is not.
# Usage: test-defrag.sh WORKDIR ENGINE DEFRAG_CHECK
set -Eeuo pipefail

here=$(cd "$(dirname "$0")" && pwd)
usage="usage: test-defrag.sh WORKDIR ENGINE DEFRAG_CHECK"
work=${1:?$usage} engine=${2:?$usage} checker=${3:?$usage}
mkdir -p "$work"
rm -f "$work"/*.img
truncate -s 64M "$work/img.img" "$work/flash.img" "$work/old.img"

rc=0
"$here/../vm/run.sh" --work "$work/machine" \
    --copy "$here/fragment.sh" --copy "$engine" \
    --disk "img=$work/img.img" --disk "flash=$work/flash.img,ssd" \
    "$here/check-defrag.sh" || rc=$?
(( rc == 0 )) || exit "$rc"

# section LOG NAME prints the lines a script put between its markers.
section() {
    sed -n "/^DFR-$2-BEGIN\$/,/^DFR-$2-END\$/{//!p}" "$1"
}
log=$work/machine/console.log
for n in 0 1 2; do
    section "$log" "REF$n" >"$work/ref.$n"
    section "$log" "EXT$n" >"$work/ext.$n"
done
section "$log" REPORT0 | base64 -d >"$work/report.0"
section "$log" DEFRAG1 | base64 -d >"$work/defrag.1"
section "$log" DEFRAG2 | base64 -d >"$work/defrag.2"

failed=0
"$checker" defrag "$work/defrag.1" deep "$work/ref.0" "$work/ref.1" \
    "$work/ext.0" "$work/ext.1" "$work/report.0" deep/frozen deep/swap || failed=1
"$checker" defrag "$work/defrag.2" "" "$work/ref.1" "$work/ref.2" \
    "$work/ext.1" "$work/ext.2" - deep/frozen deep/swap || failed=1

# The old CPU's machine runs no mkfs: the host makes the filesystem.
PATH=$PATH:/usr/sbin:/sbin mkfs.ext4 -q -F -b 4096 "$work/old.img"
"$here/../vm/run.sh" --work "$work/old-machine" --cpu qemu64 \
    --copy "$engine" --disk "img=$work/old.img" \
    "$here/check-old-cpu.sh" || rc=$?
(( rc == 0 )) || exit "$rc"
section "$work/old-machine/console.log" PROBE | base64 -d >"$work/probe.bin"
"$checker" probe "$work/probe.bin" /mnt/img || failed=1
exit "$failed"
