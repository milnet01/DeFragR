# shellcheck shell=sh
# Run in the test machine by the image_report test, with one blank disk
# called "img", the engine at /test/defragr-engine and /test/hold-lease
# (hold-lease.asm). Runs `report` on a
# fragmented image and checks what only the machine can see: the exit
# codes, and that the disk is byte for byte unchanged (S3); and which of
# several stacked or disguised mounts `probe` lists. Then prints
# the engine's output, the reference, the drive's used inodes and names,
# its free and total blocks, and each file's extents between marker lines,
# for test-report.sh to decode on the host.
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
# Nothing more is written. Mounted afresh, ext4 lets go of the blocks it
# set aside for the files just written, which statfs counts free and
# GETFSMAP does not (INV-17).
umount /mnt/img && mount -t ext4 "$dev" /mnt/img
check "the image is mounted afresh" $? 0
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
# INV-17's map: the free blocks, and the disk's 4 KiB blocks from its
# 512-byte sectors. statfs's own total leaves out the filesystem's blocks.
echo "$(stat -f -c %f /mnt/img) $(( $(cat "/sys/class/block/${dev##*/}/size") / 8 ))" \
    >/tmp/space
before=$(disk_sum)
check "the disk's sum is read" "$(echo "$before" | grep -cxE '[0-9a-f]{64}')" 1
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

# Probe judges each mount by its own id. Of two drives stacked on one
# folder, only the top one, the one visible there, is listed. A memory
# filesystem naming a drive as its source does not hide that drive: only
# btrfs's source is trusted. A snap image is no drive. Each mount point and device name appears in
# probe's output once per DRIVE record that holds it.
times_in() { grep -a -o "$1" /tmp/probe.bin | wc -l; }
truncate -s 8M /tmp/lower.img && mkfs.ext4 -q /tmp/lower.img &&
    cp /tmp/lower.img /tmp/upper.img && cp /tmp/lower.img /tmp/real.img
check "three small drives are made" $? 0
lower=$(losetup -f) && losetup "$lower" /tmp/lower.img &&
    upper=$(losetup -f) && losetup "$upper" /tmp/upper.img &&
    real=$(losetup -f) && losetup "$real" /tmp/real.img
check "three loop devices" $? 0
mkdir -p /mnt/stack /mnt/spoof /mnt/real &&
    mount -t ext4 "$lower" /mnt/stack && mount -t ext4 "$upper" /mnt/stack &&
    mount -t tmpfs "$real" /mnt/spoof && mount -t ext4 "$real" /mnt/real
check "the stacked and disguised mounts are made" $? 0
# A snap package's image: squashfs on a loop device, which passes every
# other test of a drive. snap.squashfs holds one file; it was made with
# mksquashfs in -noappend -all-root -mkfs-time 0 -all-time 0.
snap=$(losetup -f) && losetup -r "$snap" /test/snap.squashfs &&
    mkdir -p /mnt/snap && mount -t squashfs -o ro "$snap" /mnt/snap
check "the snap image is mounted" $? 0
/test/defragr-engine probe >/tmp/probe.bin
check "probe exits OK" $? 0
check "the stacked folder is listed once" "$(times_in /mnt/stack)" 1
check "the top drive is listed" "$(times_in "$upper")" 1
check "the drive under it is not" "$(times_in "$lower")" 0
check "the disguised tmpfs is not listed" "$(times_in /mnt/spoof)" 0
check "the drive it names is listed" "$(times_in /mnt/real)" 1
check "the snap image is not listed" "$(times_in /mnt/snap)" 0
/test/defragr-engine report /mnt/snap >/dev/null
check "report on the snap image exits NOT_A_MOUNT" $? 3
umount /mnt/snap
losetup -d "$snap"
umount /mnt/real
umount /mnt/spoof
umount /mnt/stack
umount /mnt/stack
# Mountinfo lists mounts oldest first, and a mount moved keeps its place.
# Moving an older drive onto a newer one puts the top drive's line first,
# the covered drive's after it. Report must still accept the drive probe
# lists there.
mkdir -p /mnt/park /mnt/flip &&
    mount -t ext4 "$upper" /mnt/park && mount -t ext4 "$lower" /mnt/flip &&
    mount --move /mnt/park /mnt/flip
check "an older drive is moved onto a newer one" $? 0
check "the top drive's line comes first" \
    "$(awk '$5 == "/mnt/flip" { for (i = 7; $i != "-"; i++); print $(i + 2) }' \
        /proc/self/mountinfo | xargs)" "$upper $lower"
/test/defragr-engine probe >/tmp/probe.bin
check "the moved drive is listed" "$(times_in "$upper")" 1
/test/defragr-engine report /mnt/flip >/dev/null
check "report on the moved drive exits OK" $? 0
umount /mnt/flip
umount /mnt/flip
losetup -d "$lower" && losetup -d "$upper" && losetup -d "$real"

# DFR-0073: what another user on the machine can do cannot stall or
# mislead a report. Each check is on a small drive of its own.
truncate -s 8M /tmp/own.img && mkfs.ext4 -q /tmp/own.img &&
    own=$(losetup -f) && losetup "$own" /tmp/own.img &&
    mkdir -p /mnt/own && mount -t ext4 "$own" /mnt/own &&
    echo data >/mnt/own/leased && mkdir -p /mnt/own/x/img
check "the small drive is made" $? 0
# A file's owner may hold a write lease on it. Opening the file then waits
# for the owner to let go, up to lease-break-time, and hold-lease never
# does. The walk must skip the file at once, not wait.
echo 30 >/proc/sys/fs/lease-break-time
/test/hold-lease /mnt/own/leased >/tmp/lease.out &
holder=$!
tries=0
while [ ! -s /tmp/lease.out ] && [ "$tries" -lt 100 ]; do
    sleep 0.1
    tries=$((tries + 1))
done
check "the lease is held" "$(cat /tmp/lease.out)" held
timeout 10 /test/defragr-engine report /mnt/own >/tmp/lease.bin
check "report beside a held lease exits OK without waiting" $? 0
check "the leased file is skipped" "$(grep -a -o leased /tmp/lease.bin | wc -l)" 1
kill "$holder"
wait "$holder"
# Else probe would list the drive here, not where the next checks look.
umount /mnt/own
check "the small drive is unmounted" $? 0
# A path that is the drive's mount point in mountinfo, but which a link in
# a parent folder leads to a sub-folder of the same mount: here the drive
# is covered by a memory filesystem whose link "p" leads, through this
# shell's open folder x, to x/img. Its mount id is the drive's; it is
# not the drive's top folder, so it is not the drive.
mkdir -p /mnt/cover/p/img && mount -t ext4 "$own" /mnt/cover/p/img &&
    exec 4</mnt/cover/p/img/x &&
    mount -t tmpfs none /mnt/cover && ln -s "/proc/$$/fd/4" /mnt/cover/p
check "the drive is covered by a link to its sub-folder" $? 0
/test/defragr-engine report /mnt/cover/p/img >/dev/null
check "report through a link to a sub-folder exits NOT_A_MOUNT" $? 3
/test/defragr-engine probe >/tmp/probe.bin
check "probe does not list a drive reached through a link" \
    "$(times_in /mnt/cover/p/img)" 0
umount /mnt/cover
exec 4<&-
umount /mnt/cover/p/img
losetup -d "$own"
# Mount points long enough, escaped, to fill more than 4 MiB of mountinfo,
# as another user's FUSE mounts could. Probe and report still work.
gap=$(printf '%240s' '')
long=/mnt/long
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do long="$long/$gap"; done
mkdir -p "$long"
mounts=0
while [ "$mounts" -lt 300 ] && mount -t tmpfs none "$long"; do
    mounts=$((mounts + 1))
done
check "300 long mounts are made" "$mounts" 300
check "mountinfo is over 4 MiB" \
    "$(( $(wc -c </proc/self/mountinfo) > 4 * 1024 * 1024 ))" 1
/test/defragr-engine probe >/tmp/probe.bin
check "probe beside a huge mountinfo exits OK" $? 0
check "probe still lists the drive" "$(times_in /mnt/img)" 1
/test/defragr-engine report /mnt/img >/dev/null
check "report beside a huge mountinfo exits OK" $? 0
while [ "$mounts" -gt 0 ] && umount "$long"; do mounts=$((mounts - 1)); done
check "the long mounts are gone" "$mounts" 0

# Every name below the mount point; the image's names hold no newline.
names=$(find /mnt/img -mindepth 1 -xdev | wc -l)

# Blocks each file occupies, for the sizes the engine reports.
(cd /mnt/img && find . -type f | sort | while read -r path; do
    echo "$(( ($(stat -c %s "$path") + 4095) / 4096 )) ${path#./}"
done) >/tmp/blocks
# Each file's extents as filefrag -v lists them: START LENGTH PATH, in
# blocks. Its rows read "N: LOGICAL..END: PHYSICAL..END: LENGTH: ...".
(cd /mnt/img && find . -type f | sort | while read -r path; do
    filefrag -v "$path" | awk -v path="${path#./}" \
        '/^ *[0-9]+:/ { gsub(/\.\./, " "); gsub(/:/, " "); print $4, $6, path }'
done) >/tmp/extents

section REFERENCE </tmp/reference
section BLOCKS </tmp/blocks
section SPACE </tmp/space
section EXTENTS </tmp/extents
base64 </tmp/report.bin | section REPORT
base64 </tmp/subdir.bin | section SUBDIR
echo "$estimate $names" | section INODES

umount /mnt/img
[ "$failures" -eq 0 ]
