#!/bin/sh
# A probe that lists one drive, /mnt/slow, then pauses until its test
# lets it finish (docs/specs/DFR-0002-engine-protocol.md § 4.4 and § 4.5;
# hold.sh), so a test can act while the drive is already sent and the run
# still going. Any other command line gets no stream.
[ "$1" = probe ] || exit 3
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

# HELLO: version 1.1, probe, block size 0.
header 1 16; le 1 2; le 1 2; le 1 4; zeros 8
# DRIVE: 1000 blocks, 500 free, 4096 bytes each, device 8:1, spinning,
# report and defrag, writable; /mnt/slow on /dev/sdz1, ext4.
header 16 80
le 1000 8; le 500 8; le 4096 4; le 8 4; le 1 4; le 2 1; le 2 1; zeros 2; zeros 8
string_part 40 /mnt/slow; string_part 56 /dev/sdz1; string_part 72 ext4
hold_sent
hold_wait
# FINISHED: OK.
header 2 8; zeros 8
