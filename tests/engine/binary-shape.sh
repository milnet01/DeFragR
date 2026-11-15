#!/usr/bin/env bash
# Fails unless the engine is a static program with a non-executable stack.
# docs/design.md: the engine depends on the kernel and nothing else.
# The assembly standard (languages/asm.md): the stack is marked non-executable.
set -Eeuo pipefail

engine=${1:?usage: binary-shape.sh ENGINE}
command -v readelf >/dev/null || { echo "readelf is not installed" >&2; exit 1; }

headers=$(readelf --program-headers --wide "$engine")
dynamic=$(readelf --dynamic "$engine")

if grep -q INTERP <<<"$headers"; then
    echo "$engine asks for a dynamic loader" >&2
    exit 1
fi
if grep -q NEEDED <<<"$dynamic"; then
    echo "$engine needs a shared library:" >&2
    grep NEEDED <<<"$dynamic" >&2
    exit 1
fi
stack=$(grep GNU_STACK <<<"$headers" || true)
if [[ -z $stack ]]; then
    echo "$engine has no GNU_STACK header, so its stack is executable" >&2
    exit 1
fi
if [[ $(awk '{print $7}' <<<"$stack") == *E* ]]; then
    echo "$engine has an executable stack: $stack" >&2
    exit 1
fi
echo "$engine is static, needs no library, and its stack is not executable"
