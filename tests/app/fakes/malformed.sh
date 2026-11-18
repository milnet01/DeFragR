#!/bin/sh
# An engine whose stream breaks a rule and which then keeps running: a
# probe's HELLO (docs/specs/DFR-0002-engine-protocol.md § 4.4), then a
# header of type 0, which no record has (§ 4.1), then it waits to be
# stopped. The window must end the run at the broken record, not when
# the engine exits.
printf '\001\000\000\000\020\000\000\000'
printf '\001\000\002\000\001\000\000\000\000\000\000\000\000\000\000\000'
printf '\000\000\000\000\000\000\000\000'
# exec, so stopping this process stops the wait (as chatter.sh).
exec sleep 30
