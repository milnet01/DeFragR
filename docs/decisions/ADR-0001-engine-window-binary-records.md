# ADR-0001: The engine and window are separate processes that talk in binary records over pipes

- **Status:** Accepted
- **Date:** 2026-11-15

## Context

The engine needs admin rights; the window must not have them. The engine
is assembly with no C library, so parsing text is costly to write and
easy to get wrong. The window must survive the engine crashing, and the
engine must stop safely if the window dies.

## Decision

The window starts the engine as a child process — through `pkexec` for
`report` and `defrag`, plainly for `probe`. The engine writes records to
its standard output and reads commands from its standard input.

Every record is a fixed header (type, length) and a little-endian
payload of fixed-width fields. Paths travel as length-prefixed bytes,
not as text. The first record states the protocol version. A reader
skips a record type it does not know, using the length.

The layout's one home is the protocol spec in `docs/specs/`.

## Consequences

- No privileged code in the window, and no window code under root.
- The engine never parses text beyond its arguments.
- The stream cannot be read by eye. Debugging needs a decoder; the
  window's session log is that decoder's output.
- Two hand-written halves of one layout can drift. The
  `selftest-records` conformance test is what catches it; without it
  this decision is unsafe.
- Each privileged action is a separate `pkexec` run, so a password may
  be asked more than once. A Polkit policy with `auth_admin_keep`
  reduces this once installed.
