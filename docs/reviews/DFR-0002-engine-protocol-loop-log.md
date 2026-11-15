# DFR-0002 — loop log

The review record for
[`docs/specs/DFR-0002-engine-protocol.md`](../specs/DFR-0002-engine-protocol.md).

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-11-15 | 2, each holding every question (`neutral-lane`) | 0 | 2 | 2 | 2 | 6 verified, 6 fixed, 0 dismissed. Q2: `tests/CMakeLists.txt`'s `engine_runs` expects exit 0 where § 4.3 gives 1 (now in § 11); verdict/reason pairing against unknown verdicts (§ 4.5, INV-9). Q3: which order rules apply was unkeyed (§ 4.10 now keys them on `HELLO.command`); an OK report could carry `block_size` 0 (§ 4.4, INV-8). Q4: self-test values were not distinct per byte, so a mis-sized field passed (§ 4.9, INV-10); the check-order row missed unprivileged cases (§ 10, INV-11). Packet pass: every citation windowed, none defective. Open questions resolved with no finding: `pkexec` 126/127 (its man page), `statfs` units (`f_bsize` equals `f_frsize` on every mount measured). |
