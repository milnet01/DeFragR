# shellcheck shell=sh
# Sourced by a fake engine to pause where its test says, not for a fixed
# time (tests/app/FakeHold.h). DEFRAGR_TEST_HOLD names a folder the test
# owns; unset, nothing here waits or writes.

# Tells the test this point is reached: creates `sent`.
hold_sent() {
    [ -n "${DEFRAGR_TEST_HOLD:-}" ] || return 0
    : >"$DEFRAGR_TEST_HOLD/sent"
}

# Waits until the test creates `go` or removes the folder; 30 s at most,
# so a test that died leaves no fake behind.
hold_wait() {
    [ -n "${DEFRAGR_TEST_HOLD:-}" ] || return 0
    tries=0
    while [ -d "$DEFRAGR_TEST_HOLD" ] && [ ! -e "$DEFRAGR_TEST_HOLD/go" ] \
        && [ "$tries" -lt 1500 ]; do
        sleep 0.02
        tries=$((tries + 1))
    done
}
