# shellcheck shell=sh
# Run in the test machine by the vm_smoke test, with two blank disks:
# "spin", attached as spinning, and "flash", attached as an SSD. Checks
# what every image test relies on: the guard, the SSD presentation, and
# that an ext4 filesystem can be made, used and checked.
# shellcheck source=SCRIPTDIR/guard.sh
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
rotational() { cat "/sys/block/${1##*/}/queue/rotational"; }

spin=$(test_disk spin)
flash=$(test_disk flash)
check "two test disks" "$(test_disks | wc -l)" 2
check "spin reads as spinning" "$(rotational "$spin")" 1
check "flash reads as an SSD" "$(rotational "$flash")" 0

# The guard accepts a test disk and refuses every other device.
check "guard accepts spin" "$(require_test_disk "$spin")" "$spin"
truncate -s 16M /tmp/loop.img
loop=$(losetup -f)
losetup "$loop" /tmp/loop.img
check "a loop device exists to refuse" "$([ -b "$loop" ] && echo yes)" yes
require_test_disk "$loop" >/dev/null 2>&1
check "guard refuses a loop device" $? 1
require_test_disk /dev/null >/dev/null 2>&1
check "guard refuses a character device" $? 1
require_test_disk /dev/nonexistent >/dev/null 2>&1
check "guard refuses a missing path" $? 1
test_disk nosuchdisk >/dev/null 2>&1
check "test_disk refuses an unknown name" $? 1
losetup -d "$loop"

# An ext4 filesystem on a test disk: made, mounted, written, checked.
mkfs.ext4 -q -F "$(require_test_disk "$spin")"
check "mkfs.ext4" $? 0
mount -t ext4 "$spin" /mnt
check "mount" $? 0
dd if=/dev/urandom of=/mnt/file bs=64k count=16 2>/dev/null
check "the file is written whole" "$(stat -c %s /mnt/file)" 1048576
sum=$(sha256sum /mnt/file)
umount /mnt
mount -t ext4 "$spin" /mnt
check "file survives a remount" "$(sha256sum /mnt/file)" "$sum"
filefrag /mnt/file | grep -Eq 'extents? found'
check "filefrag reads extents" $? 0
umount /mnt
e2fsck -fn "$spin" >/dev/null 2>&1
check "e2fsck -fn is clean" $? 0

[ "$failures" -eq 0 ]
