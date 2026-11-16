#!/bin/sh
# An engine from a future protocol major: a HELLO with major 2
# (docs/specs/DFR-0002-engine-protocol.md § 4.1 and § 4.4), then nothing.
# Header: type 1, flags 0, length 16. Payload: major 2, minor 0,
# command 1 (probe), block_size 0, reserved 0.
printf '\001\000\000\000\020\000\000\000'
printf '\002\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000'
