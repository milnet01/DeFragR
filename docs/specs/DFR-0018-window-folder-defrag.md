<!-- ants-spec-format: 1 -->
# DFR-0018 — Defrag one folder from the window, with the map animating

**Status:** accepted (2026-11-19).
**Kind:** feature.
**Source:** ROADMAP DFR-0018 (design-2026-11-15; owner request
2026-11-17 for the motion).
**Blocked by:** DFR-0014, DFR-0017.  **Blocker for:** DFR-0019, DFR-0021.
**Layman:** Choose a folder and watch its blocks join up on the map as
it is defragged.

## 1. Goal

"Defrag now" asks for a folder on the picked drive, asks once more, and
runs the engine's `defrag` on it. While it runs the window shows the
drive's map, and each move the engine reports plays on it as a short
journey. When the run ends the map shows the drive as the engine left
it, and the window says how many files were joined up, left or skipped,
and why.

## 2. Problem

`ui/MainWindow.cpp`'s "Defrag now" opens a box saying defragging comes
later (`model::messages::defragComesLater`). `EngineProcess` knows
`CommandDefrag`, and nothing starts it. `DriveMapView::changeCells` has
no caller in `src/`. `model/RunState`, which `docs/design.md` § The
parts names, does not exist.

Three facts shape the work:

1. A defrag stream held no map when this was written
   (`docs/specs/DFR-0002-engine-protocol.md` § 4.12, protocol 1.4), so
   the map came from a report, a second engine run with its own
   password prompt. DFR-0096 changed that before release: asked with
   `map`, the engine sends the map at the head of the defrag stream.
2. A drive has far more blocks than the map has cells, so one move
   often changes no cell's pattern. The motion must still show it.
3. The engine can report moves faster than anyone can watch them.

## 3. Scope decisions (agreed with the user)

- **The journey** (owner, 2026-11-17, ROADMAP DFR-0018): old cells light
  up, a trail of small blocks glides along a gentle arc to the new
  place and lands cell by cell, the landed cells glow and settle, and
  the old cells fade to free. A soft marker rests where the engine is
  working. The map trails the engine by about a second at most, small
  files settle together, and the final picture equals the drive. A
  cell's symbol changes as well as its colour, nothing flashes more
  than three times a second, and the desktop's animations setting
  turns the motion off.
- **The map comes first, in the same engine run** (owner, 2026-11-19:
  one password prompt, before 0.3.0; DFR-0096).
- **A map the window already holds for the drive is used again**
  (owner, 2026-11-19): one from a report viewed in this window, or from
  its last defrag of that drive. Otherwise the run asks the engine for
  one.
- **The folder is picked in the desktop's folder dialog** (owner,
  2026-11-19). The window lists the drive's folders to do so; it still
  opens no file there.

The rest are Claude's calls, 2026-11-19, not yet confirmed by the
owner:

- **A run cannot be paused or cancelled** until DFR-0024; the question
  before the run says so.
- **The marker rests where the latest blocks landed.** No 1.4 record
  names a file before its moves (ROADMAP DFR-0020).

## 4. Design

### 4.1 Map: moves change the tallies

`model/DriveMap` gains:

```cpp
// A move the engine made (BLOCKS_MOVED): its from blocks now count as
// free, its to blocks as a whole file's.
void move(const engine::BlocksMoved &move);
// The file those moves belonged to is done. Not in one piece: the
// blocks that landed for it count as a fragmented file's.
void endTurn(bool inOnePiece);
// How many blocks each of cells(count)'s cells holds.
quint64 blocksPerCell(int count) const;
```

A map can be stale or coarse, so a move may name blocks the map holds
as something else, or past its end. `move` then takes the shortfall
from blocks in no run, then from the other kinds, and grows the map as
`add` does. No tally goes below zero. The blocks landed in a turn are
kept per bin, so their number is bounded by `kBinsMax`.

### 4.2 Run: `model/RunState`

```cpp
// The folder as defrag's argument (DFR-0002 § 4.3): relative to the
// mount point, "" for the mount point itself. Nothing when `chosen` is
// not that folder or one below it. Compared by name; no link is read.
std::optional<QByteArray> folderOnDrive(const QByteArray &mountPoint,
                                        const QString &chosen);

class RunState : public QObject {
public:
    enum class Stage { Idle, ReadingMap, Defragging, Done, Failed };
    explicit RunState(QString launcher = QStringLiteral("pkexec"),
                      QObject *parent = nullptr);
    // A map of the drive mounted there, to use instead of reading one.
    // Ignored while a run is going.
    void setMap(const QByteArray &mountPoint, const DriveMap &map);
    bool hasMapOf(const QByteArray &mountPoint) const;
    // Ignored while a run is going.
    void start(const QByteArray &mountPoint, const QByteArray &folder);
    ...
signals:
    void stageChanged();
    void mapProgressed(quint64 names, quint64 estimate);
    void blocksMoved(const defragr::engine::BlocksMoved &move);
    void changed();   // counts or the map changed
    void ended();     // the stage is Done or Failed
};
```

- `start` runs `defrag <mount> <folder>` when it holds a map of that
  mount, and `defrag <mount> <folder> map` when it does not. The map
  is taken when `MAP_DONE` arrives. A run that ends before then ends as
  `Failed`, with no map.
- Each `BLOCKS_MOVED` goes to `DriveMap::move`, then out as
  `blocksMoved`. `FILE_DEFRAGGED` ends the turn in one piece when
  `extents_after` is 1; `FILE_SKIPPED` ends it not in one piece.
- It counts files joined up (`moved_blocks` above 0), left as they
  were (`moved_blocks` 0) and skipped, and blocks moved. It keeps the
  first `kSkippedListed` skipped files to show; the count goes on.
- A run that ends `Finished` keeps its map for the next run on that
  drive, whatever the result code: every record before `FINISHED` was
  true. Any other end drops the map.
- The message for a run that ended badly is `messages::forRun`'s.

### 4.3 Map view: the journey

`ui/DriveMapView` gains:

```cpp
static constexpr int kLaunchGapMs = 150;   // between launches
static constexpr int kFlightMs = 600;      // a small block's flight
static constexpr int kLandSpreadMs = 200;  // first landing to last
static constexpr int kMotesMax = 16;       // small blocks per launch

// A move the shown map has taken: its journey joins the next launch.
void showMove(const engine::BlocksMoved &move);
// The shown map changed without a move: the cells follow at the next launch.
void followMap();
// Off: no glow, no fade, no small blocks. Starts as the desktop says.
void setMotion(bool on);
```

- **Launches.** Moves wait for the next launch, at most `kLaunchGapMs`
  after the last. A launch reads the map's cells once, and schedules
  every cell that differs from what it will show: a from cell soon
  after the launch, a to cell as its small block lands, any other at
  `kFlightMs`. So a cell's change begins within `kLaunchGapMs +
  kFlightMs + kLandSpreadMs` of `showMove`, unless the three-a-second
  rule holds it back. Every waiting move joins one launch, so nothing
  queues up, and small files settle together.
- **The three-a-second rule is the view's own** (`kChangeGapMs`), and
  holds for scheduled changes as for `changeCells`.
- **Small blocks** fly from the from cells to the to cells, at least
  three and at most `kMotesMax` a launch, each on a curve bowed a
  little to one side. They fly whether or not a cell's pattern changes.
- **A from cell** glows in its old pattern, then fades to its new one.
  **A to cell** changes as `changeCells` changes one.
- **The marker** is a thick outline, edged in the opposite shade,
  round the last to cell of the latest launch and the cells next to it.
  It stays until `showMap` or a resize. When it moves, a second outline
  closes in on it from `kPulseReach` cells further out in `kPulseMs`:
  the pulse, begun at most once every `kPulseGapMs` (owner, 2026-11-19:
  the working spot was hard to find; ROADMAP DFR-0098).
- **Only what moved is repainted**: each frame asks for the rows of
  cells whose change is under way, the small blocks' rectangles, and
  the strips the marker's and the pulse's outlines cover.
- **Motion off**: a launch happens at once, a cell changes with no
  fade or glow, no small block flies, and the marker still shows,
  without its pulse.
  Qt 6.12 and later say so through
  `QStyleHints::accessibility()->motionPreference()`; before that, the
  style's `SH_Widget_Animation_Duration` of 0 does. On 2026-11-19, with
  KDE's `AnimationDurationFactor=0`, Qt 6.12.0 gave reduced motion and
  Breeze gave 0.
- **Colours and sizes** of the small blocks and the marker are
  `ui/Theme`'s.

### 4.4 Screens

- `ui/MainWindow`: "Defrag now" opens the folder dialog at the drive's
  mount point, then a question naming the folder with the buttons
  "Defrag this folder" and "Cancel". A folder not on the drive gets a
  box saying so. The dialog is a member a test replaces
  (`setFolderPicker`).
- `ui/DefragView` (new, part of Screens): the heading, what the run is
  doing, the map key and map, the counts, and at the end the skipped
  files with why. "Back to drives" is off while a run goes.
- `ui/ReportView` tells `MainWindow` when a report has loaded, and
  `MainWindow` hands its map to the defrag page. A loaded report of a
  drive that can be defragged shows "Defrag a folder", which does as
  "Defrag now" does (ROADMAP DFR-0100).
- `model/RunState.h` gains `countLines`, the counts as sentences, beside
  the counts as `model/Report.h` has `summaryLines`.
- `model/Messages` gains the words for each `FILE_SKIPPED` stage
  (DFR-0002 § 4.6, § 4.12), for a folder that is not on the drive, and
  for the question. `defragComesLater` goes.
- Every name the engine or the dialog gives is shown as plain text.

### 4.5 Fakes and the demo

`tests/app/fakes/window.sh` gains map runs in its report and an answer
to `defrag`. `tests/map-demo` plays its scripted changes as moves, so
the journey can be judged by eye.

## 5. Invariants

- **INV-1** — After `DriveMap::move`, the from blocks count as free and
  the to blocks as a whole file's; after `endTurn(false)` the blocks
  that landed in that turn count as a fragmented file's, and those of
  earlier turns are unchanged.
  *Test:* `tests/app/tst_drivemap.cpp::movesChangeCells`.
  *Breaks when:* `move` leaves the from cells fragmented, or `endTurn`
  reaches a file that ended whole. The fixture has one block per cell,
  so only the tally rules decide each cell.

- **INV-2** — A move naming blocks the map holds as another kind, in no
  run, or past its end leaves every cell with a state, and the to
  cells showing a whole file.
  *Test:* `tests/app/tst_drivemap.cpp::moveOnAStaleMap`.
  *Breaks when:* the shortfall is subtracted from a tally that lacks
  it (the unsigned tally wraps and the cell shows the from kind).

- **INV-3** — `RunState::start` makes one engine run: `defrag` with
  `map` when it holds no map of the drive, and without when it does.
  `defrag`'s arguments are the mount point and the folder, each as
  `EngineProcess::pathArgument` writes it.
  *Test:* `tests/app/tst_runstate.cpp`, against a fake that logs its
  command lines.
  *Breaks when:* a held map is ignored, or the map is asked for in a
  run of its own.

- **INV-4** — `folderOnDrive` gives the folder relative to the mount
  point, "" for the mount point, and nothing for a folder outside it,
  including one whose name only starts with the mount point's
  (`/mnt/GamesOld` against `/mnt/Games`) and one reached with `..`.
  *Test:* `tests/app/tst_runstate.cpp::folderOnDrive`.
  *Breaks when:* the check compares text prefixes without the `/`.

- **INV-5** — When a run ends `Finished`, `RunState`'s counts equal the
  stream's records, and its map equals the report's map with every
  `BLOCKS_MOVED` and turn end applied. A run that ends otherwise, or
  before `MAP_DONE`, holds no map afterwards.
  *Test:* `tests/app/tst_runstate.cpp`.
  *Breaks when:* a file with `moved_blocks` 0 is counted as joined up,
  or a broken stream's map is kept.

- **INV-6** — Once the motion stops after any series of `showMove` and
  `followMap`, `DriveMapView::cells()` equals the shown map's
  `cells(capacity())`.
  *Test:* `tests/app/tst_drivemapview.cpp::movesEndAsTheMap`, with
  moves arriving faster than launches and a turn ending not in one
  piece.
  *Breaks when:* a launch compares against what cells show now, not
  what they are already due to show, and so drops a change back.

- **INV-7** — A cell's change begins within `kLaunchGapMs + kFlightMs +
  kLandSpreadMs` of the `showMove` that caused it, on the view's clock,
  when the cell's last change began more than `kChangeGapMs` before.
  *Test:* `tests/app/tst_drivemapview.cpp::movesShowWithinASecond`.
  *Breaks when:* moves queue behind a journey in flight.

- **INV-8** — One cell made to change by a move every 100 ms starts at
  most three changes in any second.
  *Test:* `tests/app/tst_drivemapview.cpp::movesFlashNoMoreThanThreeASecond`,
  with motion off, where nothing but `kChangeGapMs` delays a change.
  *Breaks when:* a scheduled change skips `kChangeGapMs`.

- **INV-9** — What the screen shows after a journey is each cell's
  settled pattern, read from the window's own pixels and not from a
  fresh paint; and a frame in mid-journey paints less than the whole
  view.
  *Test:* `tests/app/tst_drivemapview.cpp::onlyWhatMovesIsRepainted`,
  reading `QScreen::grabWindow`. On 2026-11-19 the offscreen platform's
  `grabWindow` returned the window's stored pixels: after
  `update(QRect(0, 0, 100, 10))` on a repainted-blue widget it read
  blue at the top and the old red below.
  *Breaks when:* a frame leaves out a row whose cell finished changing
  since the last frame, or the small blocks' old places.

- **INV-10** — With motion off, `showMove` leaves the screen showing
  only settled patterns, at once, and no small block is painted.
  *Test:* `tests/app/tst_drivemapview.cpp::motionOffChangesAtOnce`.
  *Breaks when:* the launch still waits for `kFlightMs`.

- **INV-11** — "Defrag now" starts the engine only after a folder on
  the drive is picked and "Defrag this folder" is pressed. Cancelling
  the dialog or the question, or picking a folder elsewhere, starts
  nothing.
  *Test:* `tests/app/tst_mainwindow.cpp`, counting the fake's command
  lines.
  *Breaks when:* the run starts before the question is answered.

- **INV-12** — A file or folder name that looks like markup shows as
  written on the defrag page, in labels, the skipped list and tooltips.
  This is the trust boundary: names come from the drive, which any
  local user can write to.
  *Test:* `tests/app/tst_mainwindow.cpp::defragRunsAndShowsItsResult`.
  *Breaks when:* a label holding a name is left as `Qt::AutoText`.

- **INV-13** — Every `FILE_SKIPPED` stage from 1 to 11, and a stage
  this window does not know, has words and shows no bare code.
  *Test:* `tests/app/tst_messages.cpp::skippedStagesHaveWords`.
  *Breaks when:* a stage falls through to an empty sentence.

## 6. Failure modes

| What goes wrong | What the window does |
|---|---|
| The password is refused | `Failed`, with `messages::forRun`'s words; no map is shown |
| The run ends before `MAP_DONE` | `Failed`; no part of the map is shown |
| `defrag` ends with a result other than `OK` | `Failed` with that result's words; the counts and the map so far stay shown |
| The defrag stream breaks | `Failed`; the map is dropped and the map view emptied |
| The held map is stale | Moves still apply (INV-2); the picture can differ from the drive until the next report. **Nothing** detects it |
| The window is resized mid-run | The cells are fitted again from the map, settled; journeys in flight are dropped |
| The window is closed mid-run | `~EngineProcess` closes the pipes, which ends the engine at its next write (`docs/design.md` § Errors) |

## 7. Tests

Each is seen to fail before the code it locks is written, per
`testing.md` § 1.

| Test | Locks |
|---|---|
| `tests/app/tst_drivemap.cpp` | INV-1, INV-2 |
| `tests/app/tst_runstate.cpp` (new) | INV-3, INV-4, INV-5 |
| `tests/app/tst_drivemapview.cpp` | INV-6 to INV-10 |
| `tests/app/tst_mainwindow.cpp` | INV-11, INV-12 |
| `tests/app/tst_messages.cpp` | INV-13 |

## 8. Alternatives considered (and rejected)

- **A report first, then the defrag, as two engine runs.** Built
  first, and replaced by DFR-0096: it asked for the password twice.
- **Always read a new map.** Never stale. Rejected: DFR-0060 measured
  a cold report on `/mnt/Games` at 19 min 56 s.
- **Play every move as its own journey.** Rejected: the engine can
  outrun it, and the map would fall behind.
- **Keep each turn's to runs.** Rejected: a file's run count is the
  drive's to choose; a per-bin count is bounded.
- **Type the folder into a box.** Opens nothing on the drive. Rejected:
  it asks a partially sighted user to type a path.

## 9. Out of scope

- The status bar, progress bar and percentage — tracked by DFR-0021.
- Pause and cancel — tracked by DFR-0023 and DFR-0024.
- Translating the new sentences — tracked by DFR-0067.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1, INV-2 | `tests/app/tst_drivemap.cpp` |
| INV-3, INV-4, INV-5 | `tests/app/tst_runstate.cpp` |
| INV-6 to INV-10 | `tests/app/tst_drivemapview.cpp` |
| INV-11, INV-12 | `tests/app/tst_mainwindow.cpp` |
| INV-13 | `tests/app/tst_messages.cpp` |
| § 3 the journey's look, and that it is pleasant to watch | **nothing**: judged by eye in `tests/map-demo` |
| § 4.3 the desktop's setting turns motion off | `Partial:` INV-10 checks the view with motion off; **nothing** checks that the view reads the desktop's setting |
| § 4.3 the marker | `Partial:` `tests/app/tst_drivemapview.cpp`: `markerPulsesAtMostOnceASecond` for the pulse's pace, `onlyWhatMovesIsRepainted` for where the outline lies and that the pulse leaves nothing behind, `motionOffChangesAtOnce` for no pulse. **Nothing** checks that it is easy to find: judged by eye |
| § 6 a stale map | **nothing** |
| § 4.4 the new screens read right to left | **nothing**: checked by eye with `defragr -reverse` (`docs/design.md` § Wording and languages) |

## 11. Cross-doc impact

- `docs/design.md` § The parts: Screens gains `ui/DefragView.{h,cpp}`;
  § What may depend on what says the folder dialog lists the drive's
  folders.
- `CHANGELOG.md`: the 0.3.0 entry.
- `ROADMAP.md` DFR-0018: names this spec.
- `README.md`: only if it says defragging is not built.

## 12. Cold-eyes loop log

Rows live in `../reviews/DFR-0018-window-folder-defrag-loop-log.md`.

## 13. Resource cost

- `DriveMap`: one count per bin a turn's blocks landed in, at most
  `kBinsMax`, emptied at each turn's end.
- `RunState`: one `DriveMap`, and at most `kSkippedListed` skipped
  files, each path at most `kStringMax` bytes.
- `DriveMapView`: at most `kMotesMax` small blocks a launch, and the
  runs of the moves waiting for one launch, capped; beyond the cap a
  move's cells still change, without small blocks.
