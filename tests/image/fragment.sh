# shellcheck shell=sh
# Sourced by image tests in the test machine, as `. /test/fragment.sh`,
# after /test/guard.sh. run.sh puts it there with --copy.
#
#   build_image NAME MOUNT REFERENCE
#       Make an ext4 filesystem on test disk NAME, mount it at MOUNT and
#       fill it with scattered files. Write REFERENCE, which must not be
#       on the image. MOUNT stays mounted.
#   describe_files MOUNT
#       Print one line per regular file under MOUNT, sorted by path:
#       EXTENTS SHA256 PATH, PATH relative to MOUNT. EXTENTS is filefrag's
#       count, the named reference for the engine's.
#
# The files, each written in 4 KiB appends with an fsync after every one.
# A spacer file is appended between every two, so nearly every append lands
# as an extent of its own; the spacer is deleted at the end, leaving holes.
# The append counts differ, so a list of the worst files has one right order.
FRAGMENTED_FILES="deep/worst:64 deep/second:32 third:16 fourth:8"
# One file written in a single go, which stays in one extent.
WHOLE_FILE=whole

describe_files() {
    (cd "$1" && find . -type f | sort | while read -r path; do
        extents=$(filefrag "$path" | sed -n 's/.*: \([0-9]*\) extents\{0,1\} found$/\1/p')
        sum=$(sha256sum "$path")
        echo "${extents:-?} ${sum%% *} ${path#./}"
    done)
}

# _append_block FILE ROUND appends one 4 KiB block, from $block, and fsyncs
# it. The round number heads the block, so no two blocks are alike.
# `sync FILE` is fsync(2); Ubuntu's busybox has no fsync command.
_append_block() {
    printf '%08d' "$2" | cat - "$block" | head -c 4096 >>"$1" && sync "$1"
}

build_image() {
    dev=$(test_disk "$1") && dev=$(require_test_disk "$dev") || return 1
    # 4 KiB blocks, as on a real drive; mkfs picks 1 KiB on a small disk.
    mkfs.ext4 -q -F -b 4096 "$dev" || return 1
    mkdir -p "$2" && mount -t ext4 "$dev" "$2" || return 1
    mkdir -p "$2/deep"

    block=/tmp/fragment-block spacer=$2/spacer
    head -c 4096 /dev/urandom >"$block"
    round=1 left=1
    while [ "$left" -gt 0 ]; do
        left=0
        for entry in $FRAGMENTED_FILES; do
            [ "$round" -le "${entry#*:}" ] || continue
            _append_block "$2/${entry%:*}" "$round" || return 1
            _append_block "$spacer" "$round" || return 1
            left=1
        done
        round=$((round + 1))
    done
    rm -f "$spacer" "$block"
    head -c 262144 /dev/urandom >"$2/$WHOLE_FILE"
    sync "$2/$WHOLE_FILE" || return 1

    describe_files "$2" >"$3"
}
