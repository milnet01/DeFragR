#!/bin/sh
# An engine that answers every command as a probe: a HELLO with command
# 1, then FINISHED OK (docs/specs/DFR-0002-engine-protocol.md § 4.3 and
# § 4.4). A report that gets this stream got no report.
# HELLO: type 1, flags 0, length 16. Payload: major 1, minor 1,
# command 1 (probe), block_size 0, reserved 0.
printf '\001\000\000\000\020\000\000\000'
printf '\001\000\001\000\001\000\000\000\000\000\000\000\000\000\000\000'
# FINISHED: type 2, flags 0, length 8. Payload: result OK, errno 0.
printf '\002\000\000\000\010\000\000\000'
printf '\000\000\000\000\000\000\000\000'
