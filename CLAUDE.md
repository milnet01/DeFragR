# DeFragR — instructions for Claude Code

## Where this project is

**State:** 4 — Between items. Discovery and design are agreed
(`docs/discovery.md`, `docs/design.md`); the queue is in `ROADMAP.md`.
**In flight:** DFR-0097, waiting for the owner's choice among the
placement options in its roadmap notes. It and DFR-0117 hold
DFR-0019, release 0.3.0.

> Keep these two lines true, and no `Next:` line. Why:
> `~/.claude/workflow.md` § 1, *State 5 has no sub-states*.

## How work is done here

- **`~/.claude/workflow.md`** — the states, the gates, and what "done"
  means. Read in place. This project does not have its own copy.
- **`~/.claude/standards/`** — how to write code, tests, commits,
  documents, releases. Also read in place.

Neither is summarised here. A rule restated in two places is two rules
that will disagree.

## This project's own facts

Everything below is specific to this project, which is why it lives here
rather than in a standard.

### Stack

x86-64 NASM engine with no libc; C++23 and Qt 6 window; CMake and
CTest. `docs/design.md` § The stack, and what it rules out, is the home.

### Build and test

`./scripts/local-ci.sh` is the gate: link check, shellcheck, a fresh
CMake build in `build/ci` with warnings as errors, then CTest.
CTest boots the throwaway test machine, and the gate fails where it
cannot; `tests/vm/run.sh` says what the host needs.
`--docs` runs the link check alone. NASM's floor is in
`src/engine/CMakeLists.txt`; the version CI builds is pinned in
`scripts/local-ci.sh`.

`build/dev`, the build the owner runs, is configured with
`-DDEFRAGR_LOG=ON`: its window writes
`~/.local/state/defragr/defragr.log`, for reading when a run looked
wrong (`src/app/engine/RunLog.h`). The gate's build and the packages
leave it off.

`./scripts/ci-container.sh run [COMMIT]` is an opt-in parity check, not
part of the push gate: the same gate in a Podman image of GitHub's
runner. Run it before pushing a toolchain-sensitive change. The runner's
pins (base image, CMake, core count) live at the top of that script.

### Release

`.claude/bump.json` is the release recipe; it bumps the OBS packaging in
`packaging/obs/` too. After the tag is pushed, run
`scripts/obs-publish.sh` under `cc-job`: it uploads to OBS
(`home:milnet:DeFragR`) and waits until every repository has built the
new version. The recipe has no step that runs after the push.

### Roadmap IDs

`DFR-NNNN`, per `roadmap-format.md` § 3.5.1. Commit subjects
are `<ID>: <description>`, per `commits.md`.

### Overrides

Any place this project deliberately departs from a global standard goes
in `docs/standards/`, with the reason. If that directory is empty, there
are none.
