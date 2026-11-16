#!/bin/sh
# An engine still running when the window lets go of it: a report's HELLO
# (docs/specs/DFR-0002-engine-protocol.md § 4.4), then, after a pause,
# a burst of records of a type no reader knows (§ 4.1: type 0x7F00, no
# payload), then it waits to be stopped. The burst is still unread when
# the test destroys its EngineProcess.
printf '\001\000\000\000\020\000\000\000'
printf '\001\000\000\000\002\000\000\000\000\020\000\000\000\000\000\000'
sleep 0.3
i=0
while [ "$i" -lt 64 ]; do
    printf '\000\177\000\000\000\000\000\000'
    i=$((i + 1))
done
sleep 30
