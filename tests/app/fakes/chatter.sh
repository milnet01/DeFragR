#!/bin/sh
# An engine still running when the window lets go of it: a report's HELLO
# (docs/specs/DFR-0002-engine-protocol.md § 4.4), then, once the test
# says go, a burst of records of a type no reader knows (§ 4.1: type
# 0x7F00, no payload), then it waits to be stopped. The test stops
# reading before it says go, and destroys its EngineProcess once the
# burst is sent, so the burst is still unread then.
# shellcheck source=SCRIPTDIR/hold.sh
. "${0%/*}/hold.sh"
printf '\001\000\000\000\020\000\000\000'
printf '\001\000\000\000\002\000\000\000\000\020\000\000\000\000\000\000'
hold_wait
i=0
while [ "$i" -lt 64 ]; do
    printf '\000\177\000\000\000\000\000\000'
    i=$((i + 1))
done
hold_sent
# exec, so stopping this process stops the wait. A child sleep would
# outlive it, holding the test's standard error open, and CTest would wait
# for it to close.
exec sleep 30
