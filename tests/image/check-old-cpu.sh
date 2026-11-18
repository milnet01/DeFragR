# shellcheck shell=sh
# Run by the image_defrag test in a test machine whose CPU has no SSE4.2
# (run.sh --cpu qemu64), with a spinning test disk called "img" that holds
# an ext4 filesystem made on the host, and the engine at
# /test/defragr-engine. INV-22: defrag is refused and report still works.
# Prints probe's output between marker lines for test-defrag.sh to check
# the drive's reason. Runs nothing dynamically linked: the host's C
# library may need a newer CPU than this one.
# shellcheck source=SCRIPTDIR/../vm/guard.sh
. /test/guard.sh

failures=0
check() {
    if [ "$2" = "$3" ]; then
        echo "ok   $1"
    else
        echo "FAIL $1: expected '$3', got '$2'"
        failures=$((failures + 1))
    fi
}
engine=/test/defragr-engine

# Without this the test proves nothing: on a CPU with SSE4.2, the same
# drive allows defrag.
check "the CPU has no SSE4.2" "$(grep -c -w sse4_2 /proc/cpuinfo)" 0
dev=$(test_disk img) && dev=$(require_test_disk "$dev") &&
    mkdir -p /mnt/img && mount -t ext4 "$dev" /mnt/img
check "the drive is mounted" $? 0
"$engine" probe >/tmp/probe.bin
check "probe exits OK" $? 0
"$engine" defrag /mnt/img "" >/dev/null
check "defrag is REFUSED" $? 6
"$engine" report /mnt/img >/dev/null
check "report exits OK" $? 0
umount /mnt/img

echo "DFR-PROBE-BEGIN"
base64 </tmp/probe.bin
echo "DFR-PROBE-END"
[ "$failures" -eq 0 ]
