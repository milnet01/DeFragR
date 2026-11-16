<!-- ants-roadmap-format: 1 -->
<!-- Generated from the Ants Terminal roadmap store. Edit it with roadmap_log; hand edits are discarded by the next write. -->
<!-- ants-roadmap-id-high-water: DFR=43 -->
# DeFragR — Roadmap

> What is planned, in progress and shipped. [CHANGELOG.md](CHANGELOG.md)
> is the user-facing record of what shipped; released items stay here and
> flip to ✅.
>
> **Format:** `~/.claude/standards/roadmap-format.md`. Theme emojis
> (§ 3.4), priority bands (§ 3.5.2) and the full bullet field set (§ 3.5)
> are defined there and deliberately not restated here.

**Legend**

- ✅ Done · 🚧 In progress · 📋 Planned · 💭 Considered
- 🚫 Dropped (closed, not done)

## 0.1.0 — Read-only report

Items are broken out of `docs/design.md`; each names the discovery signs (S1–S12) it delivers.

- ✅ [DFR-0001] **Build both programs from one CMake tree.**
  Top-level `CMakeLists.txt` with `src/engine/` (NASM, static,
  `-nostdlib`) and `src/app/` (C++23, Qt 6 Widgets and D-Bus), plus
  `tests/CMakeLists.txt` under CTest. `scripts/local-ci.sh` builds and
  runs the tests. Pins the C++ standard, the Qt floor and the NASM
  3.02 floor in the build.
  **Layman:** Sets up the build so both halves of DeFragR compile with one command.
  Kind: chore.
  Source: design-2026-11-15.
  Lanes: Build.

- ✅ [DFR-0002] **Write the engine–window record protocol spec.**
  `docs/specs/DFR-0002-engine-protocol.md`: record header, the 0.1.0
    records and the pause, resume and cancel commands (later records
    come by amendment, user's choice 2026-11-15), drive verdicts and reason codes, error codes,
  versioning, and the `selftest-records` output. The one home of the
  layout (ADR-0001). A contract between two languages, so it gets
  `review-contract` before anyone builds from it. Serves S1, S3, S9.
  review-contract 2026-11-15: 2 loops of 2 (cap), 13 findings fixed,
  none deferred; spec accepted. Log:
  docs/reviews/DFR-0002-engine-protocol-loop-log.md.
  **Layman:** Writes down exactly how the engine and the window talk, before either is built.
  Kind: doc.
  Source: design-2026-11-15.
  Lanes: Protocol.

- ✅ [DFR-0003] **Engine core and protocol conformance test.**
  `kernel.inc`, `sys.asm`, `records.inc`, `records.asm`, `main.asm`:
  argument dispatch, the first and last records, exit codes, and
  `selftest-records`. Window side: `engine/Records` decoder and the
  conformance test that decodes the self-test output.
  Replaces `engine_runs` in `tests/CMakeLists.txt` (it expects exit 0; a
  bare run now exits 1, USAGE) with INV-11's conformance test (DFR-0002
  spec § 11).
  Done 2026-11-15. tst_records locks INV-1 to INV-9; each rule
  was removed in turn and its test went red. tst_records_conformance
  locks INV-10 and INV-11. Until DFR-0004 and DFR-0008, `probe` and a
  root `report` finish INTERNAL: neither can finish OK yet.
  **Layman:** Builds the engine's backbone and a test proving the window reads it correctly.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Kernel ABI, System calls, Records, Entry, Engine link.
  Blocked-by: DFR-0001, DFR-0002.

- ✅ [DFR-0004] **Engine probe: list drives and judge each one.**
  `probe.asm`: read `/proc/self/mountinfo`, `statfs` and
  `/sys/block/*/queue/rotational`; one record per drive with size,
  filesystem, SSD or spinning, and a verdict with reason code
  (ADR-0002). Engine side of S1 and S9. Unprivileged.

  Done 2026-11-15. Which mounts are drives, and the reason order, are
  in `probe.asm`'s header. A disk mounted in several places is listed
  once (the user's choice). tst_probe checks each drive against
  QStorageInfo, statvfs and sysfs, and the reason order; an engine
  listing a disk twice, skipping a partition's disk, or checking SSD
  before the filesystem type each turned it red.
  **Layman:** The engine lists every drive and says which can be defragged and why not.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Probe.
  Blocked-by: DFR-0003.

- ✅ [DFR-0005] **Window drive list and the report-or-defrag choice.**
  `engine/EngineProcess`, `model/DriveList`, `model/Messages`,
  `ui/MainWindow`, `main.cpp`. Shows the probe's drives (S1), offers
  "View report" or "Defrag now" (S2), and enables each action from the
  engine's verdict alone, with the plain reason when one is refused
  (S9, S12). Says plainly when the engine binary is missing.
  Overlaps DFR-0009, DFR-0021, DFR-0024 in `ui/MainWindow`.
  Done 2026-11-16. Both buttons follow the verdict; clicking either
  says the action comes later (the user's choice). DFR-0009 and DFR-0018
  replace those two handlers in `ui/MainWindow.cpp`. The window and the
  engine build side by side in `bin/`, where the window looks first
  when `DEFRAGR_ENGINE` is unset. tst_engineprocess, tst_drivelist and
  tst_messages lock the run endings, the actions and the wording; with
  the launcher check removed, an engine exiting 126 read as refused
  admin rights and tst_engineprocess went red.
  **Layman:** Opening the app lists your drives and lets you choose report or defrag, greying out what does not apply.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Engine link, Drives, Messages, Screens.
  Blocked-by: DFR-0004.

- ✅ [DFR-0006] **Throwaway test machine for image tests.**
  `tests/vm/`: boot a small QEMU machine, attach image files as disks,
  run a test script as root inside, return its result (ADR-0003). The
  guard refuses any block device that is not one of its disks. Settles
  whether a disk can be presented as an SSD, and whether GitHub runners
  can boot it.
  Shipped 2026-11-16: tests/vm/run.sh, guard.sh, the vm_smoke CTest. A
  ",ssd" disk reads as rotational 0 inside, and GitHub's ubuntu-26.04
  runner boots the machine.
  **Layman:** A disposable virtual PC where real defrag tests run, far from your real drives.
  Kind: test.
  Source: design-2026-11-15.
  Lanes: Test machine.
  Blocked-by: DFR-0001.

- ✅ [DFR-0007] **Build fragmented ext4 test images.**
  `tests/image/`: make a small ext4 image, fragment files on purpose by
  interleaving appends with `fsync`, and record each file's SHA-256 and
  `filefrag` extent count as the reference.
  Shipped 2026-11-16: tests/image/fragment.sh (build_image,
  describe_files) and the image_build CTest. A spacer file between appends
  puts nearly every append in its own extent.
  **Layman:** Makes small fake drives with deliberately scattered files to test on.
  Kind: test.
  Source: design-2026-11-15.
  Lanes: Image tests.
  Blocked-by: DFR-0006.

- ✅ [DFR-0008] **Engine report: fragmentation figures and worst files.**
  `walk.asm`, `extents.asm` (`FS_IOC_FIEMAP`), `report` command run
  through `pkexec`. Stays on one filesystem, follows no symlinks.
  Image test: extent counts match `filefrag`, and the image is
  unchanged afterwards (S3).
  Carries INV-12's image test: `report` on a path that is not a mount
  point finishes NOT_A_MOUNT (DFR-0002 spec § 5).
  Replaces the INTERNAL result a root `report` gives until then
  (src/engine/main.asm).
  Done 2026-11-16. walk.asm and extents.asm, with their rules in
  their headers; probe_find_mount is the mount-point check. A drive is
  known by its mount id, so the walk keeps to one mount. The
  image_report test passes; it went red when the walk updated access
  times, when the engine stopped merging extents that sit end to end,
  and when the mount check accepted any folder under a mount.
  **Layman:** The engine measures how scattered each file is without changing anything.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Walk, Extents, Entry.
  Blocked-by: DFR-0003, DFR-0004, DFR-0007.

- ✅ [DFR-0009] **Window report view.**
  `model/Report`, `ui/ReportView`: how fragmented the drive is and its
  worst files (S3). Overlaps DFR-0005 in `ui/MainWindow`.
  Done 2026-11-16. "View report" swaps the drive list for the report
  in the same window (the owner's choice): the share of files
  fragmented, average pieces, files not read, then every fragmented file
  worst first (most pieces, then biggest). While it runs: found so far,
  the latest file, time taken; real progress is DFR-0043. The title
  names the build. tst_report locks order, figures and wording;
  tst_engineprocess destroyedMidRun locks closing mid-run, which crashed
  the window once. Seen on screen by the owner on /mnt/Games: 30,608 of
  3,188,798 files fragmented; the top entries' pieces match filefrag.
  The dev build needs SELinux's bin_t label to run through pkexec
  (DFR-0042).
  **Layman:** Shows the report in the window: how fragmented the drive is and which files are worst.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Report, Report view, Screens.
  Blocked-by: DFR-0005, DFR-0008.

- 📋 [DFR-0010] **Release 0.1.0.**
  **Layman:** Publishes the first version, which can report on a drive but not defrag it.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0001, DFR-0002, DFR-0003, DFR-0004, DFR-0005, DFR-0006, DFR-0007, DFR-0008, DFR-0009, DFR-0043, DFR-0042.

- ✅ [DFR-0037] **Faster local and GitHub CI, with less drift between them.**
  `local-gate.md` § 9's levers, one per commit, each measured before
  and after: a pinned runner image and one package list (alignment),
  no C++ module scan, ccache, gentler local builds, and a NASM cache
  on GitHub. Nothing that can flake: every cache falls back to a full
  build on a miss.
  Done 2026-11-15. Local push gate: about 7 s before, 1.7 s after
  with a warm ccache (the hook's own timing record). Cold builds are
  unchanged, now at low priority on half the cores. GitHub: 57 s before;
  25 s and 42 s on cache hits, since runner speed varies. NASM's 19-21 s
  build is gone on a hit. Package install (12-22 s) is the largest step
  left; skipping `apt-get update` was rejected as a flake risk. Declined
  for now: ccache on GitHub, test parallelism (3 tests, 0.04 s).
  **Layman:** The checks before each push and on GitHub run faster, go easier on this PC, and agree with each other more.
  Kind: perf.
  Source: user-request-2026-11-15.
  Lanes: Build.

- ✅ [DFR-0038] **An opt-in local run of the gate inside an Ubuntu 26.04 container that matches GitHub's runner.**
  `local-gate.md` § 9, the warm-containers row. This PC's compiler,
  CMake and Qt differ from GitHub's, so a local green does not prove a
  GitHub green. `scripts/ci-container.sh` builds an image from CI's own
  package list and pins, and runs `scripts/local-ci.sh` in it as the
  runner's user on the runner's core count. Opt-in: not part of the
  push gate.
  Done 2026-11-15. `warm` builds the image in about two and a half
  minutes; a `run` on 09df3fb passed with the runner's CMake 4.4.5,
  Ninja 1.13.2, GCC 15.2.0-16ubuntu1 and shellcheck 0.11.0, as uid 1001
  on 4 cores, against Qt 6.10.2 (this PC: 6.12). A commit with a
  deliberate warning failed it. The NASM build moved into
  `scripts/build-nasm.sh`, which CI and the image both run.
  **Layman:** A check you can run before pushing that copies GitHub's machine, so a change that passes here also passes there.
  Kind: chore.
  Source: user-request-2026-11-15.

- ✅ [DFR-0039] **The window shows the approved DeFragR icon.**
  `packaging/defragr.svg` and `packaging/defragr.png`, as approved.
  The PNG is built into the program as the window icon; Qt needs the
  qt6-svg plugin to draw an SVG and gives a blank icon without it. The
  desktop entry that uses the SVG is the packaging item's (0.9.0).
  Done 2026-11-15: the window's `_NET_WM_ICON` reads 512 x 512 on a
  private X display; before the change it was absent.
  **Layman:** DeFragR's window now carries its own icon in the taskbar and title bar.
  Kind: feature.
  Source: user-approval-2026-11-15 (via ants-projects-hub-website).

- 📋 [DFR-0041] **Find why the image tests are slow on this PC.**
  Measured 2026-11-16: image_build took 86 s in one local gate run
  and 50 s in an earlier one; image_report took 25 to 36 s. On GitHub's
  runner each took about 2 s. Both build the same image with
  build_image, so the gap between the two local figures may point at
  the cause. Unconfirmed guess: each fsync in build_image reaches this
  PC's disk. Done when the cause is named, and fixed or recorded.
  **Layman:** Works out why the fake-drive tests take a minute or more here but seconds on GitHub.
  Kind: investigate.
  Source: user-request-2026-11-16.
  Lanes: Image tests, Test machine.

- ✅ [DFR-0042] **Say plainly when the engine cannot be started with admin rights.**
  pkexec exits 127 both when it refuses and when it cannot execute
  the program after authorising. EngineProcess reads every 127 with no
  output as RunEnd::AdminRefused, so model/Messages says "Admin rights
  were refused". Seen 2026-11-16: the dev build on /mnt/Games is
  labelled samba_share_t, SELinux (enforcing) blocked root from running
  it, and pkexec printed "Error executing ...: Permission denied".
  The window should tell the two apart, or word the message to cover
  both.
  Done 2026-11-16 by rewording, not by telling the two apart: pkexec(1)
  gives 127 for "not authorized" and for "an error occurred", and its
  error text is translated, so the window cannot split them reliably.
  model/Messages now says "DeFragR could not start its engine with admin
  rights", and names a security setting such as SELinux if it fails
  after the password. Locked by tst_messages
  adminRightsCoverABlockedEngine.
  **Layman:** After you type your password, if the system still blocks the engine, the window should say so instead of claiming the password was refused.
  Kind: fix.
  Source: user-test-2026-11-16.
  Lanes: Engine link, Messages.

- ✅ [DFR-0043] **Report progress: files checked of about how many.**
  A report sends nothing while it walks, so ui/ReportView shows only
  the fragmented files found so far and the time taken (DFR-0009). The
  owner asked for how far it has got. Needs a new report record
  (an amendment to docs/specs/DFR-0002-engine-protocol.md, new minor):
  files visited so far, and an estimate of the total, e.g. the drive's
  used inodes from statfs. walk.asm sends it every so often; the window
  turns it into a percentage bar.
  Done 2026-11-16: protocol 1.1 adds REPORT_PROGRESS (spec § 4.6,
  INV-13, INV-14). The window shows "Checked N of about M files and
  folders (P%)" and a bar held at 99% until the report ends; with no
  inode count (btrfs, vfat) the bar keeps moving and the line gives the
  count alone.
  **Layman:** While a report runs, the window shows how many files have been checked out of roughly how many, as a real percentage.
  Kind: feature.
  Source: user-request-2026-11-16.
  Lanes: Walk, Records, Report, Report view.

## 0.2.0 — Drive map

- 📋 [DFR-0011] **Engine drive map records.**
  `extents.asm`: `FS_IOC_GETFSMAP` for free space and metadata, and
  each file's extents flagged whole or fragmented, in blocks. Engine
  side of S4. Overlaps DFR-0008 in `extents.asm`.
  Adds its drive map records to `docs/specs/DFR-0002-engine-protocol.md`
  by amendment, bumping the minor version.
  **Layman:** The engine describes where everything sits on the drive, so the window can draw it.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Extents, Records.
  Blocked-by: DFR-0008.

- 📋 [DFR-0012] **Fake engine replaying recorded runs.**
  `tests/fake-engine/` replays files in `tests/fixtures/` through
  `DEFRAGR_ENGINE`, at speed or in real time. Fixtures are captured
  from image-test runs. Lets the window be built and tested without
  root.
  **Layman:** Lets the window be tested with recorded engine runs, no admin rights needed.
  Kind: test.
  Source: design-2026-11-15.
  Lanes: Recorded runs, Engine link.
  Blocked-by: DFR-0011.

- 📋 [DFR-0013] **High-contrast theme with symbols as well as colours.**
  `ui/Theme`: palette, one symbol per map cell state, text and cell
  sizes, all asked of one place (S10). Readable without relying on
  colour.
  **Layman:** Makes the map and text clear for a partially sighted user, using shapes as well as colours.
  Kind: accessibility.
  Source: design-2026-11-15.
  Lanes: Theme.
  Blocked-by: DFR-0005.

- 📋 [DFR-0014] **Drive map model and view.**
  `model/DriveMap` bins extents into cells for the window's size;
  `ui/DriveMapView` paints them with the theme, after a report (S4,
  S10). Tested against recorded runs.
  **Layman:** Draws the drive as a grid of blocks showing whole, scattered and free space.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Map, Map view.
  Blocked-by: DFR-0011, DFR-0012, DFR-0013.

- 📋 [DFR-0015] **Release 0.2.0.**
  **Layman:** Publishes the version that shows the drive map.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0010, DFR-0011, DFR-0012, DFR-0013, DFR-0014.

## 0.3.0 — Single-folder defrag with safety checks

- 📋 [DFR-0016] **CRC32C checksum in the engine.**
  `crc32c.asm` using the CPU's `crc32` instruction, checked against
  published test vectors through a self-test command.
  **Layman:** Gives the engine a fast fingerprint to prove a file is unchanged after it moves.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Checksum.
  Blocked-by: DFR-0003.

- 📋 [DFR-0017] **Engine defrag of one folder with safety checks.**
  `move.asm`: per-file checks (regular, same drive, not immutable,
  append-only or swap, more than one extent), donor file, `fallocate`,
  `EXT4_IOC_MOVE_EXT` in chunks, CRC32C before and after, change
  records. Settles whether an `O_TMPFILE` donor works. Image test:
  extents fall, every SHA-256 matches, `e2fsck -fn` clean (S7).
  Adds defrag's arguments and records to
  `docs/specs/DFR-0002-engine-protocol.md` by amendment, and the § 4.7
  check-order test that REFUSED makes reachable.
  **Layman:** The engine safely defrags the files in one folder, checking each file is unchanged.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Move, Entry.
  Blocked-by: DFR-0008, DFR-0016.

- 📋 [DFR-0018] **Window defrag of one folder, with the map animating.**
  Pick a folder, confirm, run; `model/RunState` applies change
  records and the map shows blocks turning from fragmented to whole
  (S4). Overlaps DFR-0021 in `model/RunState`.
  **Layman:** Choose a folder and watch its blocks join up on the map as it is defragged.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Run, Map, Screens.
  Blocked-by: DFR-0014, DFR-0017.

- 📋 [DFR-0019] **Release 0.3.0.**
  **Layman:** Publishes the first version that can defrag, one folder at a time.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0015, DFR-0016, DFR-0017, DFR-0018.

## 0.4.0 — Whole-drive defrag, menu, status bar and progress

- 📋 [DFR-0020] **Engine whole-drive defrag with progress records.**
  `walk.asm` from the mount root, staying on one filesystem; progress
  records with blocks done and total. Overlaps DFR-0017 in `move.asm`.
  Adds progress records to `docs/specs/DFR-0002-engine-protocol.md` by
  amendment, bumping the minor version.
  **Layman:** The engine defrags a whole drive and reports how far it has got.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Walk, Move.
  Blocked-by: DFR-0017.

- 📋 [DFR-0021] **Menu, status bar and progress bar.**
  `ui/MainWindow` menu; status bar naming the current file (S5);
  percentage and progress bar to 100% (S6); `model/RunState`.
  **Layman:** Adds a menu, shows which file is being worked on, and a progress bar that climbs to 100%.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Screens, Run.
  Blocked-by: DFR-0018, DFR-0020.

- 📋 [DFR-0022] **Release 0.4.0.**
  **Layman:** Publishes the version that defrags a whole drive with progress shown.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0019, DFR-0020, DFR-0021.

## 0.5.0 — Pause, cancel and background mode

- 📋 [DFR-0023] **Engine priorities, pause and cancel.**
  `sys.asm`: idle I/O class and lowest CPU priority; `poll` for
  commands between chunks; a closed input means cancel. Image test: a
  cancel mid-file leaves every SHA-256 matching (S8).
  **Layman:** The engine stays out of the way of other programs and stops safely when asked.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: System calls, Records, Move.
  Blocked-by: DFR-0020.

- 📋 [DFR-0024] **Pause, cancel and background mode in the window.**
  Pause, resume and cancel buttons, usable only while a run is going
  (S8, S12); `ui/TrayIcon` keeps the run going with the window hidden.
  **Layman:** Adds pause and cancel buttons, and lets the defrag carry on from the system tray.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Screens, Run.
  Blocked-by: DFR-0021, DFR-0023.

- 📋 [DFR-0025] **Release 0.5.0.**
  **Layman:** Publishes the version with pause, cancel and background running.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0022, DFR-0023, DFR-0024.

## 0.6.0 — Reports

- 📋 [DFR-0026] **Before-and-after reports, saved to a file.**
  `model/Report` compares the report before and after a run (S3, S7);
  saving writes with `QSaveFile` where the user chooses.
  **Layman:** Shows how much better the drive is after a defrag, and lets you save the report.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Report, Report view.
  Blocked-by: DFR-0009, DFR-0021.

- 📋 [DFR-0027] **Release 0.6.0.**
  **Layman:** Publishes the version with before-and-after reports.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0025, DFR-0026.

## 0.7.0 — Options

- 📋 [DFR-0028] **Options dialog.**
  `model/Settings`, `ui/SettingsDialog`: checksum on or off, text and
  cell size (S10), passed to the engine as arguments. Each option is
  enabled only where it applies (S12).
  **Layman:** Adds settings, such as bigger text and whether to double-check every file.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Settings, Settings view.
  Blocked-by: DFR-0021.

- 📋 [DFR-0029] **Release 0.7.0.**
  **Layman:** Publishes the version with options.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0027, DFR-0028.

## 0.8.0 — Time remaining and drive health

- 📋 [DFR-0030] **Time remaining.**
  `model/RunState` estimates time left from blocks moved per second,
  shown beside the progress bar.
  **Layman:** Shows roughly how long the defrag has left.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Run.
  Blocked-by: DFR-0021.

- 📋 [DFR-0031] **Drive health from UDisks2.**
  `health/DriveHealth` reads SMART status over D-Bus; the drive list
  shows it, and the window warns before defragging a failing drive.
  **Layman:** Shows whether each drive is healthy, and warns before working on a failing one.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Health, Drives.
  Blocked-by: DFR-0005.

- 📋 [DFR-0032] **Release 0.8.0.**
  **Layman:** Publishes the version with time remaining and drive health.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0029, DFR-0030, DFR-0031.

## 0.9.0 — Release candidate

- 📋 [DFR-0033] **Packaging for a GitHub release.**
  `packaging/`: Polkit policy, desktop entry, icon, and a release
  bundle installing the engine root-owned. Install on a clean machine
  from the release alone (S11).
  **Layman:** Packages DeFragR so other Linux users can install it from GitHub.
  Kind: package.
  Source: design-2026-11-15.
  Lanes: Packaging.
  Blocked-by: DFR-0024, DFR-0026, DFR-0028, DFR-0030, DFR-0031.

- 📋 [DFR-0034] **Real-drive proof on /mnt/Games.**
  Record every file's SHA-256, defrag `/mnt/Games`, check the new
  report shows fewer fragmented files and every checksum matches (S7).
  The owner chooses when, and whether a backup comes first.
  **Layman:** Defrags your real Games drive and proves every file came through unchanged.
  Kind: test.
  Source: design-2026-11-15.
  Lanes: Image tests.
  Blocked-by: DFR-0033.

- 📋 [DFR-0035] **Release 0.9.0 release candidate.**
  **Layman:** Publishes a near-final version for testing.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0032, DFR-0033, DFR-0034.

## 1.0.0 — First public release

- 📋 [DFR-0036] **Release 1.0.0.**
  The README's install steps run as written (`verify-instructions`)
  (S11).
  **Layman:** Publishes the first finished public version.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0035.

## 1.1.0 — 3D drive viewer

- 📋 [DFR-0040] **A 3D viewer of a spinning hard drive that comes apart into its parts, step by step.**
  The user's request, after 1.0.0:
  - The model is made by Blender run headless from a script. The script
    goes in `/mnt/Games/3D Engine Assets/Blender Scripts/`, the model in
    `/mnt/Games/3D Engine Assets/Models/`, each in a fitting category
    folder (`ASSET_CATEGORIES.md` there), new if none fits.
  - Buttons show the drive at stages of assembly and disassembly.
  - Match a real drive in this machine if possible, else its platter
    count. Installed HDDs, `lsblk` 2026-11-15: Seagate ST10000VX0004
    (10 TB) and ST6000DM004 (6 TB). Their platter counts are not yet
    checked.
  Blender is `blender-5.2` (5.2.3 LTS), run headless as that folder's
  README shows. Open before building: which Qt module draws 3D in the
  window is a design change (`docs/design.md`).
  **Layman:** A 3D picture of a real hard drive that spins, and buttons that take it apart and put it back together.
  Kind: feature.
  Source: user-request-2026-11-15.
