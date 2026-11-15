#!/usr/bin/env bash
# Build NASM from source and install the binary as DEST/nasm.
#
# Usage: scripts/build-nasm.sh DEST VERSION SHA256
#
# VERSION and SHA256 come from `scripts/local-ci.sh --nasm-pin`, the pin's
# one home. Two callers run this file: the CI workflow, and the container
# image scripts/ci-container.sh builds. One recipe keeps their NASM the same.
#
# nasm.us publishes no checksums, so the tarball is checked against the hash
# pinned beside the version.
set -Eeuo pipefail

dest=${1:?usage: build-nasm.sh DEST VERSION SHA256}
version=${2:?usage: build-nasm.sh DEST VERSION SHA256}
sha256=${3:?usage: build-nasm.sh DEST VERSION SHA256}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

curl -fsSL -o "$work/nasm.tar.xz" \
    "https://www.nasm.us/pub/nasm/releasebuilds/$version/nasm-$version.tar.xz"
echo "$sha256  $work/nasm.tar.xz" | sha256sum --check --strict
tar -xJf "$work/nasm.tar.xz" -C "$work"
(
    cd "$work/nasm-$version"
    ./configure --prefix=/usr/local
    make -j"$(nproc)" nasm
)
mkdir -p "$dest"
install -m 0755 "$work/nasm-$version/nasm" "$dest/nasm"
