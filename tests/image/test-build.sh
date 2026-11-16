#!/usr/bin/env bash
# The image_build test: build_image, run in the test machine on a fresh
# blank disk, passes check-build.sh.
# Usage: test-build.sh WORKDIR
set -Eeuo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=${1:?usage: test-build.sh WORKDIR}
mkdir -p "$work"
rm -f "$work/img.img"
truncate -s 64M "$work/img.img"

exec "$here/../vm/run.sh" --work "$work/machine" \
    --copy "$here/fragment.sh" --disk "img=$work/img.img" \
    "$here/check-build.sh"
