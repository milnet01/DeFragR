# Security policy — DeFragR

DeFragR has two halves. The window runs as you. The engine runs as root,
through `pkexec`, for any command that reads or changes a drive.
[docs/design.md](docs/design.md) § Safety says how the engine guards the
drive.

## Trust boundaries

Each line names what crosses and what is checked where it crosses.

- **Privilege: the window starts the engine as root.** `pkexec` asks
  for the admin password and names the engine and its arguments. The
  window runs `defragr-engine` from its own folder, or from
  `DEFRAGR_ENGINE`. It does not yet check that root owns that file, so
  whoever can replace it can run code as root once you type the
  password. DFR-0065 adds that check.
- **Process: the engine's command line comes from the window.** It
  holds a command word and a mount point. The engine rejects an unknown
  command, and a report not run as root. The mount point must be,
  byte for byte, one listed in `/proc/self/mountinfo`, and its mount a
  drive by `probe`'s rules (ADR-0002). The walk then reads that mount
  only, matched by its mount id. `pkexec` clears the window's
  environment, so none of it reaches the engine.
- **Filesystem: the engine reads names and layouts from the drive, as
  root.** Every name is opened with `openat2`, `O_PATH`, `O_NOFOLLOW`
  and `RESOLVE_NO_XDEV`. So it follows no link, enters no other mount
  and never opens a device. Only folders and regular files are then
  opened for reading. The window writes no files yet.
- **Deserialisation: the window reads the engine's records.** The
  decoder checks framing, lengths and the order of records against
  [the protocol spec](docs/specs/DFR-0002-engine-protocol.md). A
  malformed stream ends the run with an error.
- **User input: names from the drive are untrusted text.** The window
  shows every drive and file name as plain text, so a name that looks
  like markup is shown as written. The user types nothing else; they
  pick a drive and a command.
- **Network: none.** Neither half opens a socket; a search of `src/`
  for socket and Qt network calls finds none.
- **Dependency: Qt 6 runs inside the window.** The engine links no
  library, not even the C library.

### Not defended against

- Someone who already has root, or the admin password.
- A tampered kernel.

### Accepted by design

- **A report shows names from folders you cannot open.** After you give
  the admin password, the engine reads the whole drive. So the window
  can list the names of files in `/root` or another user's home folder.
  It never shows what is inside them.
- **A short gap between finding a drive and reading it.** If the drive
  were swapped for another with a reused mount id in that gap, the
  engine would read the other one. Doing that already needs root.

## Supported versions

Before 1.0, only the latest release gets security fixes.

## Reporting a vulnerability

Please do not open a public issue. Use **Report a vulnerability** on
the repository's
[Security tab](https://github.com/milnet01/DeFragR/security/advisories/new),
which only you and the maintainer can see. You will get a reply there.
