# shellcheck shell=sh
# Sourced by every script that runs in the test machine, as
# `. /test/guard.sh`. A test touches a block device only through these, so
# it cannot touch one the test machine did not attach as a test disk.
#
#   test_disks             print every test disk, one /dev path a line
#   test_disk NAME         print the test disk run.sh attached as NAME
#   require_test_disk DEV  print DEV's path if it is a test disk or a
#                          partition of one; otherwise refuse and fail

# The SCSI vendor run.sh gives every test disk, and no other device.
TEST_VENDOR=DEFRAGR

# _disk_field SYSDIR FIELD prints the field with its padding removed.
_disk_field() {
    [ -r "$1/device/$2" ] || return 1
    read -r value <"$1/device/$2" && printf '%s\n' "$value"
}

test_disks() {
    for dir in /sys/block/*; do
        [ "$(_disk_field "$dir" vendor)" = "$TEST_VENDOR" ] || continue
        echo "/dev/${dir##*/}"
    done
}

test_disk() {
    for dir in /sys/block/*; do
        [ "$(_disk_field "$dir" vendor)" = "$TEST_VENDOR" ] || continue
        [ "$(_disk_field "$dir" model)" = "$1" ] || continue
        echo "/dev/${dir##*/}"
        return 0
    done
    echo "guard: no test disk called $1" >&2
    return 1
}

require_test_disk() {
    path=$(readlink -f "$1" 2>/dev/null)
    if [ ! -b "$path" ]; then
        echo "guard: refused $1: not a block device" >&2
        return 1
    fi
    name=${path##*/}
    # A partition's sysfs directory sits inside its disk's.
    dir=/sys/class/block/$name
    [ -e "$dir/partition" ] && dir=$(readlink -f "$dir/..")
    if [ "$(_disk_field "$dir" vendor)" != "$TEST_VENDOR" ]; then
        echo "guard: refused $1: not a test disk" >&2
        return 1
    fi
    echo "$path"
}
