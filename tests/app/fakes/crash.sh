#!/bin/sh
# An engine that dies by a signal before writing anything. SIGKILL, so
# no core dump is left behind.
kill -KILL $$
