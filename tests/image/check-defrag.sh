# shellcheck shell=sh
# Run in the test machine by the image_defrag test, with a spinning test
# disk called "img", an SSD one called "flash", and the engine at
# /test/defragr-engine. Checks what only the machine can see
# (docs/specs/DFR-0002-engine-protocol.md): § 4.7's check order as root
# (INV-21); that an immutable file and a swap file in use are left as
# they were (INV-23); and, around two defrags, one of the folder deep
# and one of the whole drive, that every file reads the same, no name
# appears or goes, and e2fsck finds nothing wrong (INV-20). The first
# defrag is asked for the drive's map, with a report taken just before
# it to compare (INV-25). Then prints filefrag's view before and after
# each run, and the engine's output, between marker lines, for
# test-defrag.sh to check on the host.
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
section() { echo "DFR-$1-BEGIN"; cat; echo "DFR-$1-END"; }
engine=/test/defragr-engine

# scatter PATH:BLOCKS... appends 4 KiB blocks to each file in turn, each
# append followed by one to a spacer file, each synced, as build_image
# does, until each file has its BLOCKS. Once the spacer is deleted, nearly
# every block is a piece of its own.
scatter() {
    round=1 left=1
    while [ "$left" -gt 0 ]; do
        left=0
        for entry in "$@"; do
            [ "$round" -le "${entry#*:}" ] || continue
            head -c 4096 /dev/urandom >>"${entry%:*}" && sync "${entry%:*}" &&
                head -c 4096 /dev/urandom >>/mnt/img/spacer && sync /mnt/img/spacer ||
                return 1
            left=1
        done
        round=$((round + 1))
    done
}
# list_extents MOUNT prints each extent of each file below MOUNT as
# LOGICAL PHYSICAL LENGTH PATH, in blocks, from filefrag -v's rows, which
# read "N: LOGICAL..END: PHYSICAL..END: LENGTH: ...".
list_extents() {
    (cd "$1" && find . -type f | sort | while read -r path; do
        filefrag -v "$path" | awk -v path="${path#./}" \
            '/^ *[0-9]+:/ { gsub(/\.\./, " "); gsub(/:/, " "); print $2, $4, $6, path }'
    done)
}
# measure N writes describe_files, list_extents and every name, as they
# stand, to /tmp/ref.N, /tmp/ext.N and /tmp/names.N.
measure() {
    describe_files /mnt/img >"/tmp/ref.$1"
    list_extents /mnt/img >"/tmp/ext.$1"
    (cd /mnt/img && find . | sort) >"/tmp/names.$1"
}
# What each file reads as, without its extent count.
sums() { cut -d' ' -f2- "/tmp/ref.$1"; }

dev=$(test_disk img) || exit 1
build_image img /mnt/img /tmp/reference
check "build_image" $? 0
# A file big enough for two moves (MOVE_CHUNK_BLOCKS, move.asm, is 1024),
# and the two files the engine must leave alone, each in many pieces.
scatter /mnt/img/deep/big:1100 /mnt/img/deep/frozen:8 /mnt/img/deep/swap:16 &&
    rm /mnt/img/spacer
check "the extra files are written" $? 0
ln -s deep /mnt/img/dlink
check "a link to deep is made" $? 0
# Mounted afresh, ext4 lets go of space it set aside while writing.
umount /mnt/img && mount -t ext4 "$dev" /mnt/img
check "the image is mounted afresh" $? 0
chattr +i /mnt/img/deep/frozen
check "frozen is immutable" $? 0
chmod 600 /mnt/img/deep/swap && mkswap /mnt/img/deep/swap >/dev/null &&
    swapon /mnt/img/deep/swap
check "swap is in use" $? 0
measure 0
check "big, frozen and swap are each in many pieces" \
    "$(awk '$3 ~ /^deep\/(big|frozen|swap)$/ && $1 > 4 { n++ } END { print n }' /tmp/ref.0)" 3

# INV-21: § 4.7's order, as root. The SSD's drive would refuse defrag.
flash=$(test_disk flash) && mkfs.ext4 -q -F -b 4096 "$flash" &&
    mkdir -p /mnt/flash && mount -t ext4 "$flash" /mnt/flash &&
    mkdir /mnt/flash/sub
check "the SSD drive is made" $? 0
"$engine" defrag /mnt/flash/sub "" >/dev/null
check "a sub-folder of the refusing drive is NOT_A_MOUNT" $? 3
"$engine" defrag /mnt/flash nowhere >/dev/null
check "the refusing drive is REFUSED before its folder is looked at" $? 6
"$engine" defrag /mnt/img nowhere >/dev/null
check "a folder that does not exist is NO_FOLDER" $? 10
"$engine" defrag /mnt/img deep/../deep >/dev/null
check "a folder written with .. is NO_FOLDER" $? 10
"$engine" defrag /mnt/img /deep >/dev/null
check "a folder with a leading / is NO_FOLDER" $? 10
"$engine" defrag /mnt/img dlink >/dev/null
check "a link to a folder is NO_FOLDER" $? 10
# INV-25: the fourth word, and the folder checked before the map is read.
"$engine" defrag /mnt/img deep maps >/dev/null
check "a fourth word that is not map is USAGE" $? 1
"$engine" defrag /mnt/img nowhere map >/tmp/nowhere
check "with map, a folder that does not exist is NO_FOLDER" $? 10
check "and nothing of the map was sent first: HELLO and FINISHED alone" \
    "$(wc -c </tmp/nowhere)" 40
umount /mnt/flash

"$engine" report /mnt/img >/tmp/report.0
check "the report before the defrag exits OK" $? 0
"$engine" defrag /mnt/img deep map >/tmp/defrag.1
check "defrag of deep, with the map, exits OK" $? 0
measure 1
check "every file reads the same after deep" "$(sums 1)" "$(sums 0)"
check "no name came or went with deep" "$(cat /tmp/names.1)" "$(cat /tmp/names.0)"
check "frozen was skipped" "$(grep -a -c deep/frozen /tmp/defrag.1)" 1
check "swap was skipped" "$(grep -a -c deep/swap /tmp/defrag.1)" 1

"$engine" defrag /mnt/img "" >/tmp/defrag.2
check "defrag of the whole drive exits OK" $? 0
measure 2
check "every file reads the same after the whole drive" "$(sums 2)" "$(sums 0)"
check "no name came or went with the whole drive" "$(cat /tmp/names.2)" "$(cat /tmp/names.0)"

swapoff /mnt/img/deep/swap && chattr -i /mnt/img/deep/frozen && umount /mnt/img
check "the image is let go" $? 0
e2fsck -fn "$dev" >/tmp/fsck.out 2>&1
check "e2fsck finds nothing wrong" $? 0
[ "$failures" -eq 0 ] || cat /tmp/fsck.out

for n in 0 1 2; do
    section "REF$n" <"/tmp/ref.$n"
    section "EXT$n" <"/tmp/ext.$n"
done
base64 </tmp/report.0 | section REPORT0
base64 </tmp/defrag.1 | section DEFRAG1
base64 </tmp/defrag.2 | section DEFRAG2
[ "$failures" -eq 0 ]
