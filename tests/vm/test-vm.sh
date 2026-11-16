#!/usr/bin/env bash
# The vm_smoke test: the test machine refuses anything but an image file as
# a disk, then boots and passes smoke.sh on fresh blank disks.
# Usage: test-vm.sh WORKDIR
set -Eeuo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=${1:?usage: test-vm.sh WORKDIR}
mkdir -p "$work"
rm -f "$work"/*.img "$work/link"
truncate -s 64M "$work/spin.img" "$work/flash.img"
ln -s "$work/spin.img" "$work/link"

# refused WHAT DISK: run.sh must refuse DISK before booting.
refused() {
    local rc=0 out
    out=$("$here/run.sh" --work "$work/machine" --disk "x=$2" "$here/smoke.sh" 2>&1) || rc=$?
    if (( rc == 77 )); then printf '%s\n' "$out" >&2; exit 77; fi
    if (( rc != 125 )) || [[ $out != *"is not a regular file"* ]]; then
        printf 'FAIL run.sh attached %s (exit %d): %s\n' "$1" "$rc" "$out" >&2
        exit 1
    fi
    printf 'ok   run.sh refuses %s\n' "$1"
}
refused "a device" /dev/null
refused "a symlink" "$work/link"

exec "$here/run.sh" --work "$work/machine" \
    --disk "spin=$work/spin.img" --disk "flash=$work/flash.img,ssd" \
    "$here/smoke.sh"
