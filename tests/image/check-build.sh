# shellcheck shell=sh
# Run in the test machine by the image_build test, with one blank disk
# called "img". Checks that build_image scatters its files as it says, and
# that its reference is true.
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
extents_of() { awk -v p="$1" '$3 == p { print $1 }' /tmp/reference; }

build_image img /mnt/img /tmp/reference
check "build_image" $? 0
cat /tmp/reference
check "the reference lists every file, and only those" \
    "$(cut -d' ' -f3 /tmp/reference | tr '\n' ' ')" \
    "deep/second deep/worst fourth third whole "

# Each scattered file is in more pieces than half its appends, and each in
# fewer than the one before it, so the worst-files order is unambiguous.
previous=
for entry in $FRAGMENTED_FILES; do
    path=${entry%:*} appends=${entry#*:}
    extents=$(extents_of "$path")
    check "$path is in more than $((appends / 2)) extents" \
        "$([ "${extents:-0}" -gt $((appends / 2)) ] && echo yes)" yes
    if [ -n "$previous" ]; then
        check "$path is in fewer extents than the file before it" \
            "$([ "$extents" -lt "$previous" ] && echo yes)" yes
    fi
    previous=$extents
done
check "$WHOLE_FILE is in one extent" "$(extents_of "$WHOLE_FILE")" 1

# The reference still describes the image once it is unmounted and back.
umount /mnt/img
mount -t ext4 "$(test_disk img)" /mnt/img
check "the reference matches after a remount" \
    "$(describe_files /mnt/img)" "$(cat /tmp/reference)"
umount /mnt/img
e2fsck -fn "$(test_disk img)" >/dev/null 2>&1
check "e2fsck -fn is clean" $? 0

[ "$failures" -eq 0 ]
