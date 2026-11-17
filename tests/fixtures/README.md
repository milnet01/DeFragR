# Recorded runs

Engine output saved from real runs, for `tests/fake-engine/` to play
back to the window. Each folder is one recording. Each file in it is the
engine's output for one command, named after it: `report.bin` answers
`report`.

| Folder | Where it comes from |
|---|---|
| `image-report/` | The `image_report` test's report on its fragmented image (`tests/image/`) |

## Recording again

Record again when the engine's output changes: a new protocol version
(`docs/specs/DFR-0002-engine-protocol.md` § 4.2), or a change to the
image test's image. `tst_replay` fails on a recording the window can no
longer decode.

After `./scripts/local-ci.sh` passes, copy
`build/ci/tests/image-report/report.bin` to `image-report/report.bin`.
