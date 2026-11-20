# DFR-0018 — loop log

The review record for
[`docs/specs/DFR-0018-window-folder-defrag.md`](../specs/DFR-0018-window-folder-defrag.md).

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| amend-one-run | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0096: the map comes at the head of the defrag stream (DFR-0002 § 4.12, protocol 1.5), so § 2, § 3, § 4.2, INV-3, INV-5, § 6 and § 8 say one engine run, not a report then a defrag. Building and using it are its review. |
| amend-confirmed | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | § 3: the owner confirmed two of Claude's calls after a first real run, the map used again and the folder dialog. No design change. |
| amend-report-button | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0100: § 4.4 gains the report screen's "Defrag a folder" button. Its tests are `reportOffersADefrag` and `reportOnlyDriveOffersNoDefrag` in `tests/app/tst_mainwindow.cpp`. |
| amend-marker | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0098: § 4.3's marker is thicker, edged, goes round the cells next to it and pulses when it moves; § 10's marker row names the tests that now cover part of it. |
| amend-progress | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0099: § 4.2 gains `work()`, `percentDone` and `workLine`; § 4.4 gains the defrag page's bar; § 9 leaves DFR-0021 the status bar and whole-drive progress. |
| amend-source-outline | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0105: § 4.3 gains the source outline, the marker's line dashed, round the cell last emptied; § 10 gains its row. Its tests are `sourceOutlineIsDashed` and `onlyWhatMovesIsRepainted` in `tests/app/tst_drivemapview.cpp`. |
| amend-large-pieces | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0106: § 4.1 gains `LargePieces`, `fileState` and `endTurn(CellState)`; § 10 gains its row. No invariant is added or changed. |
| amend-fixed-room | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0110: § 4.4 gains the rule that each thing above the defrag page's map keeps its room. No invariant is added; its test is `defragRunsAndShowsItsResult` in `tests/app/tst_mainwindow.cpp`. |
| amend-skipped-room | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0113: § 4.4's fixed-room rule now covers the end of a run. The work line, bar, latest file and time share one room, which shows the skipped files afterwards, so the map keeps its size. No invariant is added; its test is `defragRunsAndShowsItsResult`, seen red first (map 78 px tall against 176). |
| amend-time-left | 2026-11-19 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0030: § 4.4 gains the time left on the time line, from `secondsLeft` and `timeLeftLine`. No invariant is added; its test is `tst_runstate.cpp::timeLeftFromThePaceSoFar`. The line on the page is not tested: it needs ten seconds of a run. |
| amend-pulse-stays | 2026-11-20 | none: no reviewer was dispatched (an amendment gets no automatic review) | – | – | – | – | DFR-0116: INV-9 gains a second test, `pulseLeavesNothingWhereTheMarkerWas` in `tests/app/tst_drivemapview.cpp`, and a third way to break. No invariant is added; the test was seen red first (pulse pieces left in a ring round the cell the marker had left). |
