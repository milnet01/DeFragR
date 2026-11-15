#!/usr/bin/env bash
# Run the CI gate inside a container that copies GitHub's runner.
#
# This PC's compiler, CMake and Qt are not the runner's, so a green
# scripts/local-ci.sh here does not prove a green on GitHub. This runs the
# same gate on the runner's toolchain, as the runner's user, on the runner's
# core count, over a clean copy of one commit. `local-gate.md` § 9, the
# warm-containers row, sets those conditions.
#
# It is opt-in: the push gate does not run it. Run it before pushing a change
# that could build differently on GitHub — a compiler, CMake, Qt or package
# change, or new warnings.
#
# Usage:
#   ./scripts/ci-container.sh warm [--refresh]   build the image (needs network)
#   ./scripts/ci-container.sh run [COMMIT]       run the gate on COMMIT (HEAD)
#
# `run` builds the image first if it is missing. The image is frozen, while
# GitHub's runner takes package updates weekly, so `warm --refresh` now and
# then rebuilds it with today's packages.
set -Eeuo pipefail
cd "$(dirname "$0")/.."

# ── What the runner has ─────────────────────────────────────────────────────
# Read from a CI run's log on 2026-11-15: runner image ubuntu-26.04
# 20261111.196, CMake 4.4.5. Change these when the runner's do.
BASE='docker.io/library/ubuntu:26.04@sha256:0d5255be560e2d2ddcfbfda88549e9e815ef29e7cd7fc62183d5709e6e2963c6'
# Tools the runner preinstalls that the gate or the image build uses.
RUNNER_PACKAGES='build-essential python3 git curl ca-certificates xz-utils'
CMAKE_VERSION='4.4.5'
# From Kitware's cmake-4.4.5-SHA-256.txt.
CMAKE_SHA256='ed85135c5a5135b45e50423cb406dea04ca3338da0b8ef3c9ede963559680be0'
# GitHub's standard Linux runner for a public repository has 4 cores.
RUNNER_CORES=4
# The checkout path on the runner.
WORKDIR=/home/runner/work/DeFragR/DeFragR

# ── What CI installs, from its one home ─────────────────────────────────────
CI_PACKAGES=$(./scripts/local-ci.sh --ci-packages)
read -r NASM_VERSION NASM_SHA256 < <(./scripts/local-ci.sh --nasm-pin)

RECIPE=scripts/ci-container/Containerfile
NAME=localhost/defragr-ci

# The tag is a hash of the whole recipe, so changing any part of it builds a
# new image rather than reusing a stale one.
TAG=$(
    {
        printf '%s\n' "$BASE" "$RUNNER_PACKAGES" "$CI_PACKAGES" \
            "$CMAKE_VERSION" "$CMAKE_SHA256" "$NASM_VERSION" "$NASM_SHA256"
        cat "$RECIPE" scripts/build-nasm.sh
    } | sha256sum | cut -c1-12
)
IMAGE=$NAME:$TAG

SCRATCH_ROOT=${DEFRAGR_SCRATCH:-/mnt/Emulators/.claude/Scratchpad/DeFragR}/ci-container

say() { printf 'ci-container: %s\n' "$*" >&2; }

if [[ ${GITHUB_ACTIONS:-} == true ]]; then
    say 'refusing to run on GitHub: CI already is the runner.'
    exit 2
fi
command -v podman >/dev/null 2>&1 || { say 'podman is not installed — nothing ran.'; exit 2; }

have_image() { podman image exists "$IMAGE"; }

warm() {
    local cache=()
    [[ ${1:-} == --refresh ]] && cache=(--no-cache)
    if have_image && (( ${#cache[@]} == 0 )); then
        say "image $IMAGE is already built."
        return 0
    fi
    say "building $IMAGE (downloads packages, CMake and NASM)."
    nice -n 10 podman build "${cache[@]}" \
        --build-arg BASE="$BASE" \
        --build-arg RUNNER_PACKAGES="$RUNNER_PACKAGES" \
        --build-arg CI_PACKAGES="$CI_PACKAGES" \
        --build-arg CMAKE_VERSION="$CMAKE_VERSION" \
        --build-arg CMAKE_SHA256="$CMAKE_SHA256" \
        --build-arg NASM_VERSION="$NASM_VERSION" \
        --build-arg NASM_SHA256="$NASM_SHA256" \
        -t "$IMAGE" -f "$RECIPE" scripts
    # Images from an older recipe are never used again.
    local old
    while read -r old; do
        [[ $old == "$IMAGE" ]] || podman rmi "$old" >/dev/null
    done < <(podman images --format '{{.Repository}}:{{.Tag}}' --filter "reference=$NAME")
}

run() {
    local commit
    commit=$(git rev-parse --verify "${1:-HEAD}^{commit}")
    if [[ -z ${1:-} && -n $(git status --porcelain) ]]; then
        say 'note: uncommitted changes are NOT in this run; it checks HEAD.'
    fi
    have_image || warm

    # A clean copy of the commit, as the runner checks it out. The gate
    # writes its build tree inside it, so this checkout stays untouched.
    mkdir -p "$SCRATCH_ROOT"
    # Global, so the EXIT trap still sees it after run() returns.
    tree=$(mktemp -d "$SCRATCH_ROOT/run.XXXXXX")
    trap 'rm -rf "$tree"' EXIT
    git clone --quiet --no-hardlinks --no-checkout . "$tree/src"
    git -C "$tree/src" checkout --quiet --detach "$commit"

    # taskset, not podman's --cpuset-cpus: rootless podman here has no
    # cpuset controller. The gate's nproc then reports the runner's count.
    local cores
    cores=$(nproc)
    (( cores > RUNNER_CORES )) && cores=$RUNNER_CORES

    say "running the gate on $(git rev-parse --short "$commit") in $IMAGE, $cores cores."
    local log=$tree/gate.log rc=0
    nice -n 10 podman run --rm --network=none \
        --userns=keep-id:uid=1001,gid=1001 \
        -e CI=true \
        -v "$tree/src:$WORKDIR:Z" -w "$WORKDIR" \
        "$IMAGE" taskset -c "0-$(( cores - 1 ))" ./scripts/local-ci.sh 2>&1 \
        | tee "$log" || rc=$?

    # An exit 0 that never reached the gate's last line is not a pass.
    if (( rc == 0 )) && ! grep -qx 'local-ci: all checks passed.' "$log"; then
        say 'the gate exited 0 but never reported a pass — treating it as a FAILURE.'
        rc=1
    fi
    if (( rc == 0 )); then
        say 'PASSED on the runner toolchain.'
    else
        say "FAILED on the runner toolchain (exit $rc)."
    fi
    return "$rc"
}

case ${1:-} in
    warm) shift; warm "$@" ;;
    run) shift; run "$@" ;;
    *) sed -n '2,/^set -Eeuo/{/^set/d;s/^# \{0,1\}//;p}' "$0" >&2; exit 2 ;;
esac
