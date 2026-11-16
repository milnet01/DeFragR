#!/bin/sh
# An engine whose report on /mnt/fake or / finds three fragmented files
# and one it could not read (docs/specs/DFR-0002-engine-protocol.md
# § 4.1, § 4.4 and § 4.6). Any other command line gets no stream, so a
# wrong argument reads as a broken engine.
case "$1 $2" in
'report /mnt/fake' | 'report /') ;;
*) exit 3 ;;
esac

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

# A string in a payload whose fixed part is $1 bytes: count, bytes, then
# zeros up to the next multiple of 8. Paths here are ASCII.
string_part() { le ${#2} 4; printf '%s' "$2"; zeros $(((8 - ($1 + 4 + ${#2}) % 8) % 8)); }
padded() { echo $((($1 + 4 + ${#2} + 7) / 8 * 8)); }

# FRAGMENTED_FILE: extents $1, size_blocks $2, path $3.
fragmented() {
    header 32 "$(padded 16 "$3")"
    le "$1" 4; zeros 4; le "$2" 8; string_part 16 "$3"
}

# HELLO: report, block size 4096.
header 1 16; le 1 2; zeros 2; le 2 4; le 4096 4; zeros 4
fragmented 3 10 music/song.flac
fragmented 40 1000 games/big.iso
fragmented 3 50 games/tie.pak
# FILE_SKIPPED: errno 13, stage 4 (read folder).
header 33 "$(padded 8 lost+found)"; le 13 4; le 4 2; zeros 2; string_part 8 lost+found
# REPORT_SUMMARY: files 200, fragmented 3, extents 250, file_blocks 5000,
# skipped 1.
header 34 40; le 200 8; le 3 8; le 250 8; le 5000 8; le 1 8
# FINISHED: OK.
header 2 8; zeros 8
