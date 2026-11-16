# DeFragR

> One line: what this does, for whom.

## Status

Early development. The window lists your drives and says which it
could defrag, and why not for the rest. Reports and defragging are not
built yet. [ROADMAP.md](ROADMAP.md) holds the plan.

## Install

(Once there is something to install.)

## Usage

(Once there is something to use.)

## Working on it

After cloning, run `./scripts/setup-hooks.sh` once. It turns on the
commit and push checks, which git does not copy with a clone.

Building needs an x86-64 Linux machine with CMake 3.28 or later, Ninja,
NASM 3.02 or later, a C++23 compiler and Qt 6.4 or later (Widgets and
D-Bus).

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

`./scripts/local-ci.sh` runs every check CI runs.
`./scripts/ci-container.sh run` runs the same checks in a Podman
container with GitHub's Ubuntu toolchain, for a change that might build
differently there.

## Documentation

| | |
|---|---|
| [ROADMAP.md](ROADMAP.md) | What is planned, in progress and shipped |
| [CHANGELOG.md](CHANGELOG.md) | What shipped, when |
| [docs/discovery.md](docs/discovery.md) | What this is for, and how we would know it works |
| [docs/design.md](docs/design.md) | The shape — the parts, and what may touch what |
| [docs/decisions/](docs/decisions/) | Why a close call went the way it did |
| [docs/specs/](docs/specs/) | The contract for one feature, where one was needed |

Paths starting `~/.claude/` point at the author's own workflow standards.
They live on the author's machine and are not published; you can skip them.

## License

[MIT](LICENSE).
