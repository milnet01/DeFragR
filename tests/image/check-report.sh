# shellcheck shell=sh
# Run in the test machine by the image_report test, with one blank disk
# called "img" and the engine at /test/defragr-engine. Runs `report` on a
# fragmented image and checks what only the machine can see: the exit
# codes, and that the disk is byte for byte unchanged (S3). Then prints
# the engine's output, the reference, and the drive's used inodes and
# names between marker lines, for test-report.sh to decode on the host.
# shellcheck source=SCRIPTDIR/../vm/guard.sh
. /test/guard.sh
# shellcheck source=SCRIPTDIR/fragment.sh
. /test/fragment.sh

failures=0
check() {
    if [ "$2" = "$3" ]; then
        echo "ok   $1"
    else
        echo "FAIL $1: expected '$3', got '$2'"
        failures=$((failures + 1))
    fi
}
# The whole disk's SHA-256, read from the disk, not from memory.
disk_sum() {
    sync && echo 3 >/proc/sys/vm/drop_caches && sha256sum <"$dev" | cut -d' ' -f1
}
section() { echo "DFR-$1-BEGIN"; cat; echo "DFR-$1-END"; }

dev=$(test_disk img) || exit 1
build_image img /mnt/img /tmp/reference
check "build_image" $? 0

# A file whose pieces lie end to end on the disk: space reserved in one
# go, then its middle written, which splits it into three extents with
# no gap between them. filefrag counts it as one piece.
fallocate -l 65536 /mnt/img/joined &&
    dd if=/dev/urandom of=/mnt/img/joined bs=4096 seek=4 count=1 \
        conv=notrunc 2>/dev/null &&
    sync /mnt/img/joined
check "joined is written" $? 0
check "joined is three extents to FIEMAP" \
    "$(filefrag -v /mnt/img/joined | grep -cE '^ *[0-9]+:')" 3
# A name the walk reads but never visits: INV-14 counts it.
ln -s whole /mnt/img/link
check "link is made" $? 0
# Two empty folders, mounted on later to put other mounts inside the drive.
mkdir /mnt/img/nested /mnt/img/auto
check "the mount folders are made" $? 0
describe_files /mnt/img >/tmp/reference
check "filefrag counts joined as one piece" \
    "$(awk '$3 == "joined" { print $1 }' /tmp/reference)" 1

# strictatime: any folder or file the engine reads would now get a new
# access time, and so change the disk. The default, relatime, updates
# one only once after each change, which describe_files has spent.
mount -o remount,strictatime /mnt/img
check "remount strictatime" $? 0
# What REPORT_PROGRESS.estimate should be (INV-14): total less free inodes.
stat -f -c '%c %d' /mnt/img >/tmp/inodes
read -r total free </tmp/inodes
estimate=$((total - free))
before=$(disk_sum)
/test/defragr-engine report /mnt/img >/tmp/report.bin
check "report exits OK" $? 0
# INV-12: a folder that exists, run as root, but is not a mount point.
/test/defragr-engine report /mnt/img/deep >/tmp/subdir.bin
check "report on a sub-folder exits NOT_A_MOUNT" $? 3
check "the disk is unchanged by report" "$(disk_sum)" "$before"
# INV-12: a mount point, but of memory, not of a drive. Its stream must be
# the sub-folder's byte for byte, which test-report.sh decodes.
mkdir -p /mnt/memory && mount -t tmpfs none /mnt/memory &&
    echo data >/mnt/memory/file
check "tmpfs is mounted" $? 0
/test/defragr-engine report /mnt/memory >/tmp/memory.bin
check "report on a tmpfs exits NOT_A_MOUNT" $? 3
check "the tmpfs report is the sub-folder's" \
    "$(sha256sum </tmp/memory.bin)" "$(sha256sum </tmp/subdir.bin)"
umount /mnt/memory
# § 4.3: the window sends the mount point escaped as mountinfo writes it.
# This one holds a byte that is not UTF-8, a space and a backslash; it
# mounts the same drive, so its stream must be the first report's.
odd=$(printf '/mnt/t\377 x\134y')
mkdir -p "$odd" && mount -o bind /mnt/img "$odd"
check "the odd name is mounted" $? 0
/test/defragr-engine report '/mnt/t\377 x\134y' >/tmp/odd.bin
check "report on an escaped mount point exits OK" $? 0
check "the escaped report is the first report" \
    "$(sha256sum </tmp/odd.bin)" "$(sha256sum </tmp/report.bin)"
umount "$odd"
# Mounts inside the drive are left out silently, so the stream must be the
# first report's. The automount point must not be woken: nobody answers
# it, so waking it would hang the engine and write a request to the pipe.
# Its daemon's process group is keeper's; the engine's is not, as autofs
# lets the daemon's own group through.
mount -t tmpfs none /mnt/img/nested && echo data >/mnt/img/nested/file
check "a tmpfs is mounted inside the drive" $? 0
mkfifo /tmp/autofs.pipe && exec 3<>/tmp/autofs.pipe
setsid sleep 600 &
keeper=$!
mount -t autofs -o "fd=3,pgrp=$keeper,minproto=5,maxproto=5,direct" \
    auto /mnt/img/auto
check "an automount point is made inside the drive" $? 0
timeout 30 /test/defragr-engine report /mnt/img >/tmp/nested.bin
check "report with mounts inside exits OK" $? 0
check "report with mounts inside is the first report" \
    "$(sha256sum </tmp/nested.bin)" "$(sha256sum </tmp/report.bin)"
check "the automount was not woken" \
    "$(timeout 1 head -c 1 <&3 | wc -c)" 0
umount /mnt/img/auto
umount /mnt/img/nested
exec 3<&-
kill "$keeper"
# Every name below the mount point; the image's names hold no newline.
names=$(find /mnt/img -mindepth 1 -xdev | wc -l)

# Blocks each file occupies, for the sizes the engine reports.
(cd /mnt/img && find . -type f | sort | while read -r path; do
    echo "$(( ($(stat -c %s "$path") + 4095) / 4096 )) ${path#./}"
done) >/tmp/blocks

section REFERENCE </tmp/reference
section BLOCKS </tmp/blocks
base64 </tmp/report.bin | section REPORT
base64 </tmp/subdir.bin | section SUBDIR
echo "$estimate $names" | section INODES

umount /mnt/img
[ "$failures" -eq 0 ]
