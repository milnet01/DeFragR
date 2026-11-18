# DeFragR

> A defragmenter for spinning ext4 drives on the Linux desktop, in a
> window that shows the drive as a map.

Version 0.2.1. [CHANGELOG.md](CHANGELOG.md) says what shipped;
[ROADMAP.md](ROADMAP.md) says what comes next.

## Status

Early development. The window lists your drives and says which it
could defrag, and why not for the rest. It can report how fragmented a
drive is, list its worst files and show the drive as a map. Defragging
is not built yet.
[ROADMAP.md](ROADMAP.md) holds the plan.

## Languages

The window speaks your desktop's language: English, Hebrew, German,
Italian, Greek, Spanish, Portuguese (Brazil and Portugal), Chinese
(Simplified and Traditional), Japanese, Russian, Polish, French or
Ukrainian. Hebrew reads right to left.

Every language but English was drafted by an AI, and no native speaker
has checked it yet. Corrections are welcome as a GitHub issue or pull
request; the files are in `src/app/translations/`. A sentence added
since shows in English until it is translated.

## Install

There is no package yet. Build it from source, as
[Working on it](#working-on-it) shows. The window and its engine land
side by side in `build/bin/`.

Reading a drive needs admin rights, which DeFragR asks for through
`pkexec`. Most desktops ship it with Polkit.

## Usage

```sh
./build/bin/defragr
```

1. Pick a drive from the list. The window says whether DeFragR could
   defrag it, and why not if it could not.
2. Choose **View report** and type the admin password when asked.
3. The report shows how fragmented the drive is, its worst files, and a
   map of the drive.

The report changes nothing on the drive.

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
