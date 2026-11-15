# ADR-0002: Only the engine decides whether a drive or file may be defragged

- **Status:** Accepted
- **Date:** 2026-11-15

## Context

The window must list drives and show only the actions that apply (S1,
S12) before asking for a password. The engine must refuse an SSD, btrfs
or unsupported filesystem whatever it is told (S9). Written twice, in
assembly and C++, those rules would disagree sooner or later, and the
window would offer a defrag the engine refuses.

## Decision

The rules live in `src/engine/probe.asm` only. The window runs
`defragr-engine probe` without admin rights; the engine reads
`/proc/self/mountinfo`, `statfs` and `/sys/block/*/queue/rotational`,
and sends one record per drive with its verdict and a reason code.

The window shows the verdict and enables actions from it. The engine
re-runs the same rules at the start of every `report` and `defrag`.

## Consequences

- One home for the rule, and the window cannot offer an action the
  engine would refuse, short of the drive changing in between.
- Every rule change is an assembly change, even a cosmetic one.
- The window cannot list drives if the engine binary is missing; it
  must say so plainly.
- Reason codes are protocol, so a new refusal reason is a protocol
  change and a new `model/Messages` entry.
