#!/bin/sh
# An engine for the window's own test (tst_mainwindow). Its probe lists
# two drives whose names look like markup, and its report on the first
# finds one fragmented file whose name looks like markup, then pauses
# before the end until the test lets it finish (hold.sh), so the test can
# look at the window mid-run. Any other command line gets no stream.
#   /mnt/<img src=x>            ext4, report and defrag
#   /mnt/other  on <b>zfs</b>   not ext4, report only
# Records: docs/specs/DFR-0002-engine-protocol.md § 4.
case "$1" in
probe) ;;
report) [ "$2" = '/mnt/<img src=x>' ] || exit 3 ;;
*) exit 3 ;;
esac
# shellcheck source=SCRIPTDIR/hold.sh
. "${0%/*}/hold.sh"

# One byte with the value $1.
byte() { printf '%b' "\\0$(printf '%o' "$1")"; }

# $1 little-endian, in $2 bytes.
le() {
    value=$1 left=$2
    while [ "$left" -gt 0 ]; do
        byte $((value & 255))
        value=$((value >> 8)) left=$((left - 1))
    done
}

zeros() { le 0 "$1"; }

# Header: type $1, flags 0, payload length $2.
header() { le "$1" 2; zeros 2; le "$2" 4; }

# A string starting $1 bytes into its payload: count, bytes, then zeros
# up to the next multiple of 8. Names here are ASCII.
string_part() { le ${#2} 4; printf '%s' "$2"; zeros $(((8 - ($1 + 4 + ${#2}) % 8) % 8)); }

# The bytes a string takes: its count, itself, and the zeros after.
string_size() { echo $(((4 + ${#1} + 7) / 8 * 8)); }

# DRIVE of 1000 blocks, 500 free, 4096 bytes each, spinning: mounted at
# $1 from $2 with filesystem $3, verdict $4, reason $5.
drive() {
    header 16 $((40 + $(string_size "$1") + $(string_size "$2") + $(string_size "$3")))
    le 1000 8; le 500 8; le 4096 4; le 8 4; le 1 4; le 2 1; le "$4" 1; le "$5" 2; zeros 8
    string_part 40 "$1"
    string_part $((40 + $(string_size "$1"))) "$2"
    string_part $((40 + $(string_size "$1") + $(string_size "$2"))) "$3"
}

# FRAGMENTED_FILE: extents $1, size_blocks $2, path $3.
fragmented() {
    header 32 $((16 + $(string_size "$3")))
    le "$1" 4; zeros 4; le "$2" 8; string_part 16 "$3"
}

if [ "$1" = probe ]; then
    # HELLO: version 1.1, probe, block size 0.
    header 1 16; le 1 2; le 1 2; le 1 4; zeros 8
    drive '/mnt/<img src=x>' /dev/sdz1 ext4 2 0
    drive /mnt/other /dev/sdz2 '<b>zfs</b>' 1 1
else
    # HELLO: version 1.2, report, block size 4096.
    header 1 16; le 1 2; le 2 2; le 2 4; le 4096 4; zeros 4
    # REPORT_PROGRESS: names 0 of an estimated 300.
    header 35 16; le 0 8; le 300 8
    fragmented 3 10 '<img src=x>.bin'
    # REPORT_PROGRESS: names 150 of an estimated 300.
    header 35 16; le 150 8; le 300 8
    hold_sent
    hold_wait
    # REPORT_SUMMARY: files 200, fragmented 1, extents 3, file_blocks 5000,
    # skipped 0.
    header 34 40; le 200 8; le 1 8; le 3 8; le 5000 8; le 0 8
fi
# FINISHED: OK.
header 2 8; zeros 8
