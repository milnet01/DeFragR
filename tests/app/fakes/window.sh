#!/bin/sh
# An engine for the window's own test (tst_mainwindow). Its probe lists
# two drives whose names look like markup, and its report on either
# finds one fragmented file whose name looks like markup, then pauses
# before the end until the test lets it finish (hold.sh), so the test can
# look at the window mid-run. The report maps the drive's 1000 blocks. Its
# defrag of any folder on the first drive moves one file's 40 blocks,
# pauses in the same way, then joins that file up, leaves a second as it
# was and skips a third, saying how far it has got before the first and
# after each. Asked for the map, the defrag sends the report's map first. Any other command line gets no stream.
#   /mnt/<img src=x>            ext4, report and defrag
#   /mnt/other  on <b>zfs</b>   not ext4, report only
# DEFRAGR_TEST_LOG, when set, names a file that gains each command line.
# DEFRAGR_TEST_DEFRAG changes how the defrag ends: "mismatch" stops it at
# a file that reads back differently; "broken" breaks its stream; "no-map"
# ends it with a drive error before its map is whole; "large" ends its
# first file in two large pieces.
# Records: docs/specs/DFR-0002-engine-protocol.md § 4.
[ -z "${DEFRAGR_TEST_LOG:-}" ] || printf '%s\n' "$*" >>"$DEFRAGR_TEST_LOG"
case "$1" in
probe) ;;
report) [ "$2" = '/mnt/<img src=x>' ] || [ "$2" = /mnt/other ] || exit 3 ;;
defrag) [ "$2" = '/mnt/<img src=x>' ] || exit 3 ;;
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

# MAP_RUNS of kind $1: then each run's start and length.
map_runs() {
    kind=$1
    shift
    header 36 $((8 + 8 * $#))
    le "$kind" 4; le $(($# / 2)) 4
    for number in "$@"; do le "$number" 8; done
}

# BLOCKS_MOVED: $1 from runs, then every run's start and length, the
# from runs first.
blocks_moved() {
    from=$1
    shift
    header 65 $((8 + 8 * $#))
    le "$from" 4; le $(($# / 2 - from)) 4
    for number in "$@"; do le "$number" 8; done
}

# FILE_DEFRAGGED: extents before $1, after $2, moved_blocks $3, path $4.
defragged() {
    header 64 $((16 + $(string_size "$4")))
    le "$1" 4; le "$2" 4; le "$3" 8; string_part 16 "$4"
}

# FILE_SKIPPED: errno $1, stage $2, path $3.
skipped() {
    header 33 $((8 + $(string_size "$3")))
    le "$1" 4; le "$2" 2; zeros 2; string_part 8 "$3"
}

# The map: a whole file, a file in two pieces, the filesystem's own
# blocks, and free space.
the_map() {
    map_runs 3 0 100
    map_runs 4 100 20 300 20
    map_runs 2 120 80
    map_runs 1 200 100 320 680
}

# DEFRAG_PROGRESS: $1 files done of 3, $2 blocks done of 160.
work() { header 66 32; le "$1" 8; le 3 8; le "$2" 8; le 160 8; }

if [ "$1" = defrag ]; then
    # HELLO: version 1.7, defrag, block size 4096.
    header 1 16; le 1 2; le 7 2; le 3 4; le 4096 4; zeros 4
    if [ "${4:-}" = map ]; then
        # REPORT_PROGRESS: names 0, then 300, of an estimated 300.
        header 35 16; le 0 8; le 300 8
        the_map
        header 35 16; le 300 8; le 300 8
        if [ "${DEFRAGR_TEST_DEFRAG:-}" = no-map ]; then
            # FINISHED: DRIVE_ERROR, errno 5, before the map is whole.
            header 2 8; le 5 4; le 5 4
            exit 5
        fi
        # MAP_DONE.
        header 37 8; zeros 8
    fi
    work 0 0
    blocks_moved 2 100 20 300 20 400 40
    if [ "${DEFRAGR_TEST_DEFRAG:-}" = broken ]; then
        # A second HELLO breaks the stream's order.
        header 1 16; le 1 2; le 4 2; le 3 4; le 4096 4; zeros 4
        exit 0
    fi
    hold_sent
    hold_wait
    if [ "${DEFRAGR_TEST_DEFRAG:-}" = large ]; then
        # It ends in two pieces of 16 MiB: 8192 blocks of 4 KiB.
        defragged 2 2 8192 'games/<img src=x>.bin'
    else
        defragged 2 1 40 'games/<img src=x>.bin'
    fi
    work 1 40
    if [ "${DEFRAGR_TEST_DEFRAG:-}" = mismatch ]; then
        blocks_moved 1 320 10 500 10
        skipped 0 11 'games/bad.bin'
        # FINISHED: CHECKSUM_MISMATCH.
        header 2 8; le 11 4; zeros 4
        exit 11
    fi
    defragged 3 3 0 'games/full.bin'
    work 2 100
    skipped 0 10 'games/<b>busy</b>.bin'
    work 3 160
elif [ "$1" = probe ]; then
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
    the_map
    hold_sent
    hold_wait
    # REPORT_SUMMARY: files 200, fragmented 1, extents 3, file_blocks 5000,
    # skipped 0.
    header 34 40; le 200 8; le 1 8; le 3 8; le 5000 8; le 0 8
fi
# FINISHED: OK.
header 2 8; zeros 8
