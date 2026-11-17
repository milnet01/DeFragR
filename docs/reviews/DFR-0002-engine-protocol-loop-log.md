# DFR-0002 — loop log

The review record for
[`docs/specs/DFR-0002-engine-protocol.md`](../specs/DFR-0002-engine-protocol.md).

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-11-15 | 2, each holding every question (`neutral-lane`) | 0 | 2 | 2 | 2 | 6 verified, 6 fixed, 0 dismissed. Q2: `tests/CMakeLists.txt`'s `engine_runs` expects exit 0 where § 4.3 gives 1 (now in § 11); verdict/reason pairing against unknown verdicts (§ 4.5, INV-9). Q3: which order rules apply was unkeyed (§ 4.10 now keys them on `HELLO.command`); an OK report could carry `block_size` 0 (§ 4.4, INV-8). Q4: self-test values were not distinct per byte, so a mis-sized field passed (§ 4.9, INV-10); the check-order row missed unprivileged cases (§ 10, INV-11). Packet pass: every citation windowed, none defective. Open questions resolved with no finding: `pkexec` 126/127 (its man page), `statfs` units (`f_bsize` equals `f_frsize` on every mount measured). |
| 2 | 2026-11-15 | 2, each holding every question (`neutral-lane`) | 0 | 1 | 4 | 2 | 7 verified, 7 fixed, 0 dismissed. Q2: stream-order rules against unknown record types (§ 4.10 now lets `Unknown` sit anywhere; INV-2 fixture names command 2). Q3: no end-of-input call (§ 4.10 adds `finish()`); `StreamError` undefined (now an enum); `FINISHED.errno` sign; INV-12's test absent from DFR-0008's item (§ 11). Q4: § 10's check-order row cited a test `REFUSED` cannot reach under `report` (now DFR-0017's); INV-9's enable-defrag clause had no decoder test (moved to § 4.5, DFR-0005's). Cap reached (2 of 2 for a spec): these 7 fixes are read by no lane; implementation is their reader. Calm cap: 1 of 7 landed on this run's own text (§ 10 row). No second share: the armed change is the whole document. Status set to accepted. |
| amend-1.1 | 2026-11-16 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0043 adds `REPORT_PROGRESS` (§ 4.6), bumps the minor to 1 (§ 4.2, § 4.4, § 4.9), and adds INV-13 (decoder) and INV-14 (engine, image test). Building and using it are its review. |
| amend-INV-12 | 2026-11-17 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0044: `NOT_A_MOUNT` now also covers a mount point that is not a drive by probe's test (§ 4.7); INV-12 and its test add the tmpfs case |
