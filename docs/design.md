# DeFragR — Design

> **Purpose — so the shape is decided once, and anyone can tell where a
> new piece of work belongs and what it is allowed to touch.**

**This document is a gate.** Work is not broken into items until it is
agreed — `~/.claude/workflow.md` § 4 says when it passes.

**Status:** agreed (2026-11-15).

## The shape in one paragraph

DeFragR is two programs. The **engine** is a small x86-64 assembly
program with no C library. It is the only thing that reads extents or
moves data, and it runs with admin rights. The **window** is a C++/Qt 6
app that runs as the normal user. It starts the engine, sends it
commands, and draws what the engine reports. They share nothing but the
**record protocol** on the engine's standard input and output.

## The parts

### Engine — `src/engine/`

| Part | Responsible for | Files |
|---|---|---|
| Kernel ABI | Syscall and ioctl numbers, struct layouts, `errno` values. Changes only when a new kernel call is used | `kernel.inc` |
| System calls | Thin wrappers that return the kernel's result unchanged; process setup (I/O priority, CPU priority, signal handling) | `sys.asm` |
| Records | Encoding outgoing records and decoding incoming commands; the engine's half of the protocol | `records.inc`, `records.asm` |
| Entry | Reading arguments, choosing the command (`probe`, `report`, `defrag`), exit codes | `main.asm` |
| Probe | Listing mounted drives; deciding whether each can be defragged, and why not | `probe.asm` |
| Walk | Visiting every regular file under a folder without leaving its filesystem or following symlinks | `walk.asm` |
| Extents | Reading a file's extents (`FS_IOC_FIEMAP`) and the drive's whole map (`FS_IOC_GETFSMAP`) | `extents.asm` |
| Move | Defragging one file: safety checks, a donor file, `fallocate`, `EXT4_IOC_MOVE_EXT` in chunks, the checksum compare | `move.asm` |
| Checksum | CRC32C over a file, using the CPU's `crc32` instruction | `crc32c.asm` |

### Protocol — `docs/specs/`

The record layout is a spec, written by its roadmap item. It is the one
contract between two languages, so it is the one home of every record's
shape. `src/engine/records.inc` and `src/app/engine/Records.h` follow it.

### Window — `src/app/`

| Part | Responsible for | Files |
|---|---|---|
| Engine link | Starting the engine (plainly for `probe`, through `pkexec` otherwise), feeding it commands, turning its output into records, noticing it died; the developers' log of a run, in a build made with `DEFRAGR_LOG` | `engine/EngineProcess.{h,cpp}`, `engine/Records.{h,cpp}`, `engine/RunLog.{h,cpp}` |
| Drives | The drive list from `probe`, and which actions each drive allows | `model/DriveList.{h,cpp}` |
| Map | Turning extents into map cells, and change records into cell changes | `model/DriveMap.{h,cpp}` |
| Run | Progress, current file, counts, time remaining, pause and cancel state | `model/RunState.{h,cpp}` |
| Report | Fragmentation figures and worst files; saving a report | `model/Report.{h,cpp}` |
| Settings | Stored options | `model/Settings.{h,cpp}` |
| Messages | Every engine error code and refusal reason, in plain words with what to do; how a path is shown | `model/Messages.{h,cpp}` |
| Health | Drive health from UDisks2 over D-Bus | `health/DriveHealth.{h,cpp}` |
| Theme | Colours, symbols and sizes for the map and text | `ui/Theme.{h,cpp}` |
| Screens | Main window, start choice, the defrag page, menu, status bar, progress bar, tray icon | `ui/MainWindow.{h,cpp}`, `ui/DefragView.{h,cpp}`, `ui/FixedRoom.h`, `ui/ReadOnlyTable.h`, `ui/TrayIcon.{h,cpp}` |
| Map view | Painting and animating the map | `ui/DriveMapView.{h,cpp}` |
| Report view | Showing a report | `ui/ReportView.{h,cpp}` |
| Settings view | The options dialog | `ui/SettingsDialog.{h,cpp}` |
| Languages | Choosing the desktop's language at start-up, loading its translations and Qt's own, and setting the reading direction | `ui/Languages.{h,cpp}`, `translations/` |
| Start-up | `main()` | `main.cpp` |

### Build, packaging and tests

| Part | Responsible for | Files |
|---|---|---|
| Build | One CMake build for both programs | `CMakeLists.txt`, `src/engine/CMakeLists.txt`, `src/app/CMakeLists.txt`, `tests/CMakeLists.txt` |
| Packaging | Polkit policy, desktop entry, icon, release bundle | `packaging/` |
| Window tests | Qt Test unit tests of the model parts, run without root | `tests/app/` |
| Recorded runs | Saved engine output, replayed by a fake engine | `tests/fixtures/`, `tests/fake-engine/` |
| Map demo | A recorded map with a made-up defrag playing on it, to judge the map's motion by eye | `tests/map-demo/` |
| Image tests | Building throwaway ext4 images and running the real engine on them | `tests/image/` |
| Test machine | Booting the throwaway virtual machine the image tests run in | `tests/vm/` |

A part listed with a version it is not yet needed in is created by the
item that first needs it, not before.

## What may depend on what

**The engine depends on the kernel and nothing else.** No C library, no
other program, no file outside the drive it was given and `/proc` and
`/sys`. One exception: `probe` reads the status of a btrfs mount's
source under `/dev`, without opening it, to learn its device.

**The engine and the window share only the protocol spec.** Neither
includes the other's files.

**Only the engine touches a drive.** The window never opens a file on
the target drive, never calls an ioctl on it, and never moves data. One
exception: the folder dialog lists the drive's folders, so a folder to
defrag can be picked.

**Only the engine decides whether a drive or a file may be defragged**
([ADR-0002](decisions/ADR-0002-engine-decides-eligibility.md)). The
window shows the engine's verdict and its reason code. It enables or
hides actions from that verdict alone (S12). It never re-derives it.

**Inside the engine:**

- `main.asm` may call every part.
- `move.asm` may call `extents.asm`, `crc32c.asm`, `records.asm` and
  `sys.asm`.
- `walk.asm`, `extents.asm`, `probe.asm` and `crc32c.asm` call only
  `sys.asm` and `records.asm`. None of them calls another of them.
- `sys.asm` calls nothing but the kernel.
- Every part may include `kernel.inc` and `records.inc`.

**Inside the window, the layers point one way: `ui/` → `model/` and
`health/` → `engine/`.**

- `engine/` knows nothing of `model/` or `ui/`.
- `model/` and `health/` use Qt Core and Qt D-Bus only, never Qt
  Widgets, so every model part runs in a test without a screen.
- Only `ui/` uses Qt Widgets.
- Only `ui/Theme` chooses a colour, symbol or size. Every view asks it.
- Only `model/Messages` holds user-facing text about an engine result.

**Tests:**

- Only `engine/EngineProcess` reads `DEFRAGR_ENGINE`, which points the
  window at the fake engine.
- Image tests run only inside the test machine, and refuse any block
  device that is not a test disk the test machine created.

## What every part does the same way

### Errors

**The engine checks every system call.** A negative return is an error.

- **A file-level error** skips that file. The engine sends a
  `file skipped` record with the `errno` and the stage that failed, then
  goes on.
- **A drive-level error** ends the run. The engine sends a `finished`
  record with a non-zero result code, then exits with that code.
- **The engine writes no free text.** Its standard output carries only
  records. Its standard error is for a crash and nothing else.
- **A closed standard input means cancel.** A window that dies stops the
  engine at its next safe point.

**The window never shows a raw code.** `model/Messages` turns each code
into what failed and what the user can do (`coding.md` § 2).

**The window treats an unknown record type as skippable** and a
malformed record as a broken engine. A broken engine ends the run with a
message, never a guess.

### Safety

**Data moves only through `EXT4_IOC_MOVE_EXT`.** The kernel copies,
checks and swaps the blocks, one chunk per call.

**Before moving a file**, `move.asm` confirms it is a regular file on
the drive being defragged, not immutable or append-only, and in more
than one extent; the kernel refuses a swap file. **The drive** must
still pass `probe`'s rules at run start.

**The engine checksums each file before and after its move** with
CRC32C, reading it from the drive, not the cache. A mismatch stops the
run with an error naming the file. This is on by default; an option may
turn it off. A CPU without SSE4.2 has no `crc32` instruction, so its
drives are report only.

**The donor file is created with `O_TMPFILE`**, so a crash leaves
nothing behind. `EXT4_IOC_MOVE_EXT` accepts it: the `image_defrag` test
moves every file through one.

**Pause and cancel act between chunks.** One chunk is one kernel call,
which is atomic. A cancelled file is left valid, partly defragged.

**Paths from the window are not trusted.** The engine opens each
name with `openat2`, `O_NOFOLLOW` and `RESOLVE_NO_XDEV`, so it follows
no link and enters no other mount.

### Sharing the PC (S8)

The engine sets its I/O priority to the idle class and its CPU priority
to the lowest at start-up. It reads commands without blocking, using
`poll` with a zero timeout, between chunks.

### State and persistence

- **The engine keeps no state between runs.** Every run starts from
  what is on the drive. There is no resume file.
- **Options** are stored by `QSettings` under the user's config folder.
- **Saved reports** go where the user chooses, written with `QSaveFile`.
- **A session log** of every record received is written under
  `$XDG_STATE_HOME/defragr/`, replaced each run. It is what a bug report
  attaches.

### Units

The engine reports positions and lengths in filesystem blocks. Its
first record states the block size. The window converts for display.

### Wording and languages (S13)

**Every word the window shows goes through Qt's translation**:
`tr()` in a widget, `QCoreApplication::translate()` elsewhere. Only
review catches a bare English string.

- **A sentence is translated whole.** Values go in as `%1`, `%2`, never
  by joining pieces, because word order differs between languages. The
  one exception is a sentence with two counts: a count with its noun,
  such as "200 files", goes in as a value, and a note to the translator
  says where.
- **A sentence with a count uses Qt's plural forms**: the count goes to
  `tr()` beside the sentence. Code never picks "file" or "files".
  English's forms live in `translations/defragr_en.ts`; the
  `translations_current` test fails when one is missing.
- **Numbers, sizes and percentages are written by `QLocale`.**
- **Names stay as they are.** Paths, drive names and filesystem names
  are never translated. A path goes through `messages::path`, which
  keeps it reading left to right in a right-to-left window.
- **Every screen works right to left.** Check each new one with
  `defragr -reverse`. Nothing checks this automatically.
- **The engine needs no translation.** It writes no text (§ Errors).

Translations come once the wording has settled (0.9.0). Until then a
new sentence is English only, which is what a missing translation
shows.

## The stack, and what it rules out

| Choice | Why | Runner-up | Rules out |
|---|---|---|---|
| NASM, x86-64, no libc, statically linked, for the engine | The owner's choice. The engine is small and its kernel calls are few | C with no libc, which is easier to read and test | Other CPUs (ARM, 32-bit). Threads. Any library: no JSON, no locale, no name lookup. Engine code cannot be unit-tested in isolation, so it is tested through the protocol |
| C++23, Qt 6 Widgets and Qt D-Bus, for the window | Mature widgets, painting fit for an animated map, D-Bus for drive health | Qt Quick, which animates more easily but adds QML | A browser-based UI. Any library beyond Qt |
| `pkexec` for admin rights | Ships with every Polkit desktop; asks with the desktop's own dialog | A long-running root helper service, which is more to secure | Running the window as root. Defragging without a desktop session |
| Pipes and binary records between the two ([ADR-0001](decisions/ADR-0001-engine-window-binary-records.md)) | Assembly writes fixed fields easily and parses text badly | Text lines | Reading the stream by eye without a decoder |
| CMake with CTest | Builds NASM and C++ in one tree | Make | — |
| Image tests inside a throwaway virtual machine ([ADR-0003](decisions/ADR-0003-image-tests-in-a-vm.md)) | Root without a password prompt, and no real drive in reach | Loop devices on the host with `sudo` | Running the image tests where virtualisation is unavailable |

**What the stack rules out for later work:** XFS support needs its own
move ioctl and a new engine part. A command-line mode needs only a
second reader of the protocol, not engine changes.

## How it is tested

1. **Window unit tests** (`tests/app/`, Qt Test, no root): the record
   decoder, map binning, progress and time remaining, the message table,
   and which actions a drive allows.
2. **Protocol conformance**: the engine has a hidden `selftest-records`
   command that emits every record type with fixed values. A window test
   decodes it and checks each field. This is what keeps the two halves
   of the protocol in step.
3. **Recorded runs** (`tests/fixtures/`, `tests/fake-engine/`, no root):
   saved engine output, replayed at speed through `DEFRAGR_ENGINE`. The
   map, status bar and progress are built and checked against these.
4. **Image tests** (`tests/image/`, inside the test machine): a script
   makes a small ext4 image, mounts it, and fragments files on purpose by
   interleaving appends with `fsync`. It records each file's SHA-256 and
   its extent count from `filefrag`. Then it runs the real engine and
   checks:
   - `report` extent counts match `filefrag`, the named reference
     (`testing.md` § 11);
   - after `defrag`, extent counts fell, every SHA-256 matches, and
     `e2fsck -fn` is clean;
   - a cancelled run leaves every file matching its SHA-256;
   - an SSD-like test disk and a btrfs image get a report and a refusal
     (S9).

   The test machine presents a test disk as an SSD through its SCSI
   rotation rate (`tests/vm/run.sh`).

## Build order

Each step places in the parts above.

| Version | Delivers | Parts it adds or changes |
|---|---|---|
| 0.1.0 | Read-only report | Kernel ABI, System calls, Records, Entry, Probe, Walk, Extents; Engine link, Drives, Report, Messages; Screens (drive list, start choice, report); Build; Window tests; Image tests; Test machine |
| 0.2.0 | Drive map; every word ready for translation | Extents (`GETFSMAP`); Map, Map view, Theme; Recorded runs; Languages |
| 0.3.0 | Single-folder defrag with safety checks | Move, Checksum; Run |
| 0.4.0 | Whole-drive defrag, menu, status bar, progress | Walk (whole drive); Screens; Run |
| 0.5.0 | Pause, cancel, background mode | System calls (priorities, `poll`); Records (commands); Run; Tray icon |
| 0.6.0 | Reports | Report (save, before and after); Report view |
| 0.7.0 | Options | Settings, Settings view |
| 0.8.0 | Time remaining and drive health | Run; Health |
| 0.9.0 | Release candidate, translated | Packaging; Languages (the translations); the whole |
| 1.0.0 | Release | the whole |

## Where each sign of success is delivered

| Sign | Delivered by |
|---|---|
| S1 drive list | Probe; Drives; Screens |
| S2 report or defrag choice | Screens |
| S3 report, changes nothing | Walk, Extents; Report, Report view |
| S4 animated map | Extents, Move; Map, Map view, Theme |
| S5 current file in status bar | Move; Run; Screens |
| S6 percentage and progress bar | Move; Run; Screens |
| S7 fewer fragments, checksums match | the whole |
| S8 PC usable; pause and cancel safe | System calls, Move, Records; Run; Screens |
| S9 SSD, btrfs or unsupported gets a reason | Probe; Messages |
| S10 readable by a partially sighted user | Theme, Map view |
| S11 install from a GitHub release | the whole |
| S12 only applicable actions available | Probe; Drives; Screens |
| S13 the desktop's language, Hebrew right to left | Languages; every part that shows words |

## Close calls

- [ADR-0001](decisions/ADR-0001-engine-window-binary-records.md) —
  separate processes talking binary records over pipes.
- [ADR-0002](decisions/ADR-0002-engine-decides-eligibility.md) — only
  the engine decides what may be defragged.
- [ADR-0003](decisions/ADR-0003-image-tests-in-a-vm.md) — image tests
  run as root inside a throwaway virtual machine.
