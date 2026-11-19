<!-- ants-roadmap-format: 1 -->
<!-- Generated from the Ants Terminal roadmap store. Edit it with roadmap_log; hand edits are discarded by the next write. -->
<!-- ants-roadmap-id-high-water: DFR=104 -->
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

Items are broken out of `docs/design.md`; each names the discovery signs it delivers.

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

- ✅ [DFR-0010] **Release 0.1.0.**
  2026-11-16: 0.1.0 committed (5d70f14), tagged v0.1.0 and pushed;
  GitHub CI green on it.
  Shipped 2026-11-17: the GitHub release page is published, with the
  0.1.0 CHANGELOG section verbatim as the notes and no assets.
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

- ✅ [DFR-0041] **Find why the image tests are slow on this PC.**
  Measured 2026-11-16: image_build took 86 s in one local gate run
  and 50 s in an earlier one; image_report took 25 to 36 s. On GitHub's
  runner each took about 2 s. Both build the same image with
  build_image, so the gap between the two local figures may point at
  the cause. Unconfirmed guess: each fsync in build_image reaches this
  PC's disk. Done when the cause is named, and fixed or recorded.
  Review 2026-11-16 (review-code --performance lane 03):
  tests/vm/run.sh:169 rebuilds the whole initramfs each boot, though
  only /test differs (vm_smoke's whole run took 1.99 s, so at most about
  6 s per push). Three cheap checks: whether KVM or the TCG fallback
  runs (run.sh:173-176); the push gate's worktree sits on
  /mnt/Emulators, a spinning drive; GENTLE wraps QEMU. Note
  tst_engineprocess (30.85 s) is the slowest test, not an image test:
  see its own item.
  Cause, 2026-11-17: QEMU's default disk cache passed each of the
  machine's flushes on to the host drive, and this PC's build trees sit
  on spinning drives. build_image fsyncs after every 4 KiB append. KVM
  was in use, not TCG. With cache=unsafe on the test disks, image_build
  took about 2 s against 19 to 23 s without it, on the same drive. Fixed
  in tests/vm/run.sh; after the fix image_build took 2.61 s and
  image_report 3.24 s. The initramfs rebuild is not worth changing:
  vm_smoke's whole run took 2.20 s.
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
  Seen on screen by the owner on /mnt/Emulators, 2026-11-16 (build
  cda670d): mid-run it showed 811,019 of about 2,645,884 files and
  folders, with the bar at 30%; it finished at 2,563,555 files checked,
  11,322
  fragmented.
  **Layman:** While a report runs, the window shows how many files have been checked out of roughly how many, as a real percentage.
  Kind: feature.
  Source: user-request-2026-11-16.
  Lanes: Walk, Records, Report, Report view.

## 0.2.0 — Drive map

- ✅ [DFR-0066] **Every word ready for translation.**
  Groundwork for S13, per docs/design.md § Wording and languages.
  Every shown word goes through tr() or translate(); counts use Qt's
  plural forms, with English's forms in translations/defragr_en.ts and a test
  that fails when one is missing. ui/Languages loads the desktop's
  translation and Qt's own, and sets the reading direction from the
  language, so Hebrew mirrors. First in 0.2.0, so the map's words are
  written ready. CI gains Qt's translation tools. No translations yet
  (DFR-0067).
  Shipped 2026-11-17. Plural sentences pass their count to tr() and
  print it as %1, since %n holds only an int. Languages takes Qt's
  translations folder as a parameter, so tst_languages proves the
  right-to-left line on a PC with Qt's Hebrew installed. tst_report's
  wording failed with "file(s)" before English was wired; the
  translations_current check failed on a missing form and on a stale
  sentence. Gate green here (Qt 6.12) and in the runner container
  (Qt 6.10.2). The -reverse check found scrambled paths: DFR-0069.
  **Layman:** The app can be translated, and switches to the desktop's language and reading direction by itself.
  Kind: feature.
  Source: user-request-2026-11-17.
  Lanes: Languages, Messages, Report, Drives, Screens, Report view, Build.

- ✅ [DFR-0069] **Paths read left to right in a right-to-left window.**
  Found checking DFR-0066 with `defragr -reverse` (2026-11-17): the
  drive list showed "/mnt/Games/" as "mnt/Games/" with the slash moved,
  and sizes as "GiB 200.62". A path in a right-to-left layout needs
  marking as left-to-right text: the drive column, the report's heading,
  "Latest:", its File column, and the missing-engine message. Check
  sizes again once Hebrew exists, since Hebrew's own size units may
  read correctly. Check with `defragr -reverse`.
  Shipped 2026-11-17. model::messages::path wraps a path in Unicode's
  left-to-right isolate, and all five places use it. The tests failed
  on the bare paths first. A throwaway right-to-left render showed
  "opt/defragr/defragr-engine/" in a table cell and a Hebrew sentence
  before the fix, and the path as written after. Sizes wait for
  DFR-0067's Hebrew check.
  **Layman:** In Hebrew, a folder like /mnt/Games must still read /mnt/Games, not with its slashes moved.
  Kind: fix.
  Source: user-request-2026-11-17.
  Lanes: Drives, Report, Report view, Messages.

- ✅ [DFR-0011] **Engine drive map records.**
  `extents.asm`: `FS_IOC_GETFSMAP` for free space and metadata, and
  each file's extents flagged whole or fragmented, in blocks. Engine
  side of S4. Overlaps DFR-0008 in `extents.asm`.
  Adds its drive map records to `docs/specs/DFR-0002-engine-protocol.md`
  by amendment, bumping the minor version.
  Done 2026-11-17: protocol 1.2 adds MAP_RUNS (spec § 4.6), runs of
  blocks marked free, the filesystem's, a whole file or a fragmented
  one. extents_space reads GETFSMAP; extents_count sends a file's runs
  on a second read, once its pieces give its kind. The image test
  checks the runs block by block against statfs and filefrag -v; it
  went red when the engine stopped after one GETFSMAP call and when it
  marked every file whole. Blocks ext4 sets aside for files being
  written are in no run until the drive is mounted afresh.
  **Layman:** The engine describes where everything sits on the drive, so the window can draw it.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Extents, Records.
  Blocked-by: DFR-0008.

- ✅ [DFR-0012] **Fake engine replaying recorded runs.**
  `tests/fake-engine/` replays files in `tests/fixtures/` through
  `DEFRAGR_ENGINE`, at once or paced over a chosen number of
  seconds. Fixtures are captured from image-test runs, which finish
  too fast for their own timing to be worth keeping. Lets the window be built and tested without
  root.
  **Layman:** Lets the window be tested with recorded engine runs, no admin rights needed.
  Kind: test.
  Source: design-2026-11-15.
  Lanes: Recorded runs, Engine link.
  Blocked-by: DFR-0011.

- ✅ [DFR-0013] **High-contrast themes with symbols as well as colours, built; a user can choose one once DFR-0021's menu lands.**
  `ui/Theme`: palette, one symbol per map cell state, text and cell
  sizes, all asked of one place (S10). Readable without relying on
  colour.
  Owner ruling 2026-11-16: discovery S10 (readable by a partially
  sighted user) binds from 0.2.0, with this theme, not today's screens.
  Shipped 2026-11-17. Owner rulings that day: themes, chosen from the
  menu when DFR-0021 builds it; Desktop is the default; text follows
  the desktop's size with a floor; patterns, not marks. Seven themes:
  Desktop, Dark and Light high contrast, Yellow on black, Ocean, Ember,
  Aurora. tst_theme checks contrast, patterns read with no colour, and
  the text floor.
  Record corrected 2026-11-18 (verify-delivery; owner ruling): the seven
  themes exist in src/app/ui/Theme.cpp, but src/app/main.cpp:18 always
  builds Theme::Id::Desktop and nothing lets a user choose, so no user can
  get a high-contrast theme until DFR-0021's menu (0.4.0). No code change
  now.
  **Layman:** The map shows shapes as well as colours now; the high-contrast colour themes for a partially sighted user can be chosen once the menu arrives.
  Kind: accessibility.
  Source: design-2026-11-15.
  Lanes: Theme.
  Blocked-by: DFR-0005.

- ✅ [DFR-0014] **Drive map model and view.**
  `model/DriveMap` bins extents into cells for the window's size;
  `ui/DriveMapView` paints them with the theme, after a report (S4,
  S10). Tested against recorded runs.
  Owner request 2026-11-17: the map should be mesmerising to
  watch. This item builds the motion; DFR-0018 drives it from real
  moves. A cell never jumps: each change eases over a fraction of a
  second, from one frame timer that runs only while something moves.
  When a report arrives, the map fills in as a sweep from the first
  block to the last. Tested with scripted cell changes, so the motion
  can be watched and judged before the engine can defrag.
  Owner, 2026-11-17: they read the screen through a magnifier lens, a
  small square at a time. Each cell must read on its own, and the key
  to the patterns must be readable close to the map, not only at a far
  edge. ui::CellState lives in ui/Theme.h until model/DriveMap needs
  it; move it to model/ then. Paint cells with Theme::paintCell, sized
  by Theme::cellSide and Theme::cellGap.
  Done 2026-11-17: model/DriveMap sorts MAP_RUNS into cells; a cell
  shows the kind filling most of it, and blocks in no run count as the
  filesystem's. CellState moved there; ui/Theme.h aliases it.
  ui/DriveMapView fades each change over 400 ms and sweeps a new map in
  over 1.5 s, from one frame timer that stops when still; right to left
  in a right-to-left language. mapKey puts the patterns and their names
  right above the map. tst_drivemap and tst_drivemapview (on a test
  clock) cover it. tests/map-demo plays the recorded run with scripted
  changes, which sidesteps pkexec stripping DEFRAGR_RECORDING for
  watching the motion. Turning off motion with the desktop's
  animations stays with DFR-0018.
  **Layman:** Draws the drive as a grid of blocks showing whole, scattered and free space.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Map, Map view.
  Blocked-by: DFR-0011, DFR-0012, DFR-0013.

- ✅ [DFR-0073] **Engine: an ordinary user cannot stall or block a report.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  Robustness and security fixes: a local user without admin rights can
  hold up, break or mislead the root engine's report. Three defects in
  src/engine/walk.asm and src/engine/probe.asm, two MEDIUM and one LOW.
  Per SECURITY.md, their locations, causes and fix shapes are not on this
  public page; the fixer reads them in .claude/private/security-2026-11-18.md
  on the owner's machine (untracked). Each fix is proven in the test
  machine, seen red first.
  Found by review-code lane 02.
  Priority: 1 — any local user can deny reports to everyone.
  Shipped 2026-11-18. All three fixed. Each has a check in image_report,
  seen to fail before its fix and pass after; hold-lease.asm is a test
  helper for one of them. probe's output on this machine is byte for
  byte the same.
  **Layman:** Stops another user on the same computer from freezing or breaking a drive report.
  Kind: review-fix.
  Source: code-quality-review-2026-11-18.
  Lanes: Walk, Probe.

- ✅ [DFR-0074] **Window: file names cannot spoof the screen, and a report's memory is bounded.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  - Two places where a file or drive name from the drive can change how
    the window shows text, against SECURITY.md's "a name that looks like
    markup is shown as written": one in src/app/ui/ReportView.cpp
    (MEDIUM) and one in src/app/model/Messages.cpp with
    src/app/model/DriveList.cpp (LOW). Per SECURITY.md, the locations
    and fix shapes are not on this public page; the fixer reads them in
    .claude/private/security-2026-11-18.md on the owner's machine
    (untracked). Each fix gets a test seen red first.
  - `src/app/model/Report.cpp:141` and `:145` — every FRAGMENTED_FILE (a
    path up to 4 KiB) and every MAP_RUNS run is kept for the whole run,
    uncapped, and a local user who can write to the drive chooses how
    many. Files: the owner chose to list every file (DFR-0051,
    2026-11-17); this review adds that a local user can grow that list,
    so ask the owner again before changing it. Map runs: measure memory
    per run on /mnt/Games, then hold them in a bounded form that
    DriveMap::cells can still re-bin on resize (S4). (MEDIUM.)
  Found by review-code lanes 03 and 04.
  Priority: 1 — names on a drive are untrusted (SECURITY.md).
  Shipped 2026-11-18. Both name fixes have a test seen to fail first
  (tst_mainwindow's tooltip check; tst_messages' direction-control and
  fsType checks). The owner chose on 2026-11-18 to list every file up to
  a memory ceiling, then say how many more were found: Report keeps the
  worst files within 512 MiB (path plus 64 bytes each) and filesHeading
  names the rest. DriveMap no longer keeps runs: it tallies blocks per
  kind into at most 2^18 bins, 8 MiB, exact up to that many blocks, so no
  per-run measurement on /mnt/Games was needed. tst_report and
  tst_drivemap's new checks failed before the change (3 rows; 64 MB held).
  **Layman:** A file's name can never disguise itself or scramble what the screen says, and a huge drive cannot use up the computer's memory.
  Kind: review-fix.
  Source: code-quality-review-2026-11-18.
  Lanes: Report view, Messages, Report, Drives.

- ✅ [DFR-0075] **Map view enforces the three-flashes-a-second limit itself.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  - `src/app/ui/DriveMapView.h:7` says "nothing flashes more than three
    times a second", but `changeCells` (DriveMapView.cpp:128-151)
    enforces nothing: a change mid-glow restarts it
    (`next.append({shown, cells[i], now, kChangeMs})`). The glow peaks at
    180 ms and glowAt is about 0.8 by 100 ms, so a cell changed every
    100 ms brightens about 10 times a second.
  - `DriveMapView.cpp:214` — resizeEvent calls sortMap, which re-indexes
    the cells against the old m_sweepStart, so dragging the window edge
    during the 2.5 s sweep restarts glows at the same screen spot.
  Owner ruling 2026-11-18: enforce WCAG 2.2 SC 2.3.1's limit inside
  DriveMapView, so it holds for any caller (DFR-0018 will feed engine
  changes straight in). Fix shape: keep each cell's last glow start; a
  change within 334 ms of it changes the state without a new glow, or
  defers the glow; a resize re-sort starts no glow. Test in
  tst_drivemapview on the view's own clock: change one cell every 100 ms
  and count glow starts per second (at most 3). Latent today: changeCells
  has no caller in src/ (map-demo at 700 ms and the tests only).
  DFR-0071's record says "One glow per change keeps S10's limit"; once
  this lands, that record must say the view enforces it.
  Found by review-readability lane 04 (HIGH) and review-code lane 04
  (MEDIUM, calibrated LOW: no caller in src/).
  Priority: 1 — a safety limit the view claims and does not keep.
  Shipped 2026-11-18. The fix defers rather than drops the glow: a plain
  change of pattern many times a second is a flash too. A change asked for
  less than kChangeGapMs (334 ms) after the cell's last change began waits
  until then; a later request replaces it. A resize sorts the map into
  settled cells, so it starts no glow and stops the sweep. tst_drivemapview:
  one cell asked to change every 100 ms starts 3 changes in a second (10
  before the fix); a resize mid-sweep paints the same as a settled map
  (it did not before). DFR-0071's record now says the view enforces it.
  **Layman:** The drive map can never flash fast enough to harm someone sensitive to flashing light, whatever later feeds it.
  Kind: review-fix.
  Source: review-readability-2026-11-18.
  Lanes: Map view.

- ✅ [DFR-0076] **Tests that pass when the behaviour they check is broken.**
  Confirmed by review-tests' orchestrator 2026-11-18 at 6e9b702 (mutations
  M1–M6, reading or a run); do not re-review (review-cost.md § 4).
  - `tests/app/tst_engineprocess.cpp:119` (also :126, :142;
    tst_replay.cpp:120) — RunOutcome::end defaults to RunEnd::Broken and
    the run() helpers only qWarning when ended() never comes
    (tst_engineprocess.cpp:35, tst_replay.cpp:41), so the 126, crash,
    wrong-command (report half) and no-recording tests pass if ended()
    never fires. Fix: make each helper's wait a hard check, or assert
    ends == 1 as launcherRefusal does. (MEDIUM, calibrated from HIGH.)
  - `tst_engineprocess.cpp:89` — launcherRunsTheEngine checks only that
    the run finished and HELLO says report; a launcher dropping the `/`
    still passes. Fix: also assert finished.result == ResultNotRoot, with
    a reasoned QSKIP as root (as tst_records_conformance.cpp:122). (LOW.)
  - `tests/app/tst_drivelist.cpp:147` — the loop runs zero times if
    loaded() brings no rows. Fix: assert rowCount() > 0, with a reasoned
    QSKIP when the host has none (as tst_probe.cpp:107). (LOW.)
  - `tests/app/tst_drivemapview.cpp:214` — each rowsMap() row is one
    state, so top right equals left-to-right cell 9; deleting cellRect's
    right-to-left branch stays green. Fix: a map whose first block differs
    from its row; assert it is drawn top right, not top left. (HIGH.)
  - `tests/app/tst_mainwindow.cpp:267` — no row is selected, and
    updateActions disables View report with no selection, so the listing
    branch is untested (mutation M2 stayed green). Fix: select row 0
    before refresh->click(). Trap (verify-delivery 2026-11-18): in a
    right-to-left window QTableView::selectRow() does not select the row
    (mouse and keyboard do), so a Hebrew variant must select by click or
    key. (MEDIUM.)
  - `tests/app/tst_messages.cpp:127` — checks only that no digit shows;
    errorText falling back to "no details given", or a dropped %1, both
    pass. Fix: also assert `what` contains
    QString::fromLocal8Bit(std::strerror(13)). (MEDIUM.)
  - `tests/vm/smoke.sh:47` — dd's status is unchecked; an empty /mnt/file
    passes the hash and "extents found" checks (lines 51-53). Fix: check
    dd's status, or that the file is 1 MiB. (LOW.)
  - `tests/image/check-report.sh:26` — a failing step in disk_sum prints
    nothing (sh, no pipefail), so line 78's "disk unchanged" compares two
    empty strings. Fix: require $before to be 64 hex characters. (LOW.)
  DFR-0063's record claims each check was seen to fail; :267 was not.
  Found by review-tests lanes 1-3.
  Priority: 2 — fixes after this are proven by these tests.
  Shipped 2026-11-18. Each fix was seen red with its behaviour broken:
  - tst_engineprocess (126, crash, wrong command) asserts ends == 1;
    tst_replay noRecordingIsBroken asserts ended. Red with ended() not
    emitted for a broken run.
  - launcherRunsTheEngine asserts ResultNotRoot (QSKIP as root). Red with
    the arguments dropped on the launcher path.
  - loadsFromTheEngine asserts rowCount() > 0; it skips only when the
    engine's own probe, asked directly, lists no drives. Red with
    probeEnded showing no drives.
  - rightToLeftStartsTopRight uses a map whose first block alone is
    fragmented. Red with cellRect's right-to-left branch removed.
  - refreshIsGreyedWhileListing picks drive 0 before Refresh drives. Red
    with the listing branch's m_report->setEnabled(false) removed (M2).
  - errnoIsWords asserts strerror(13)'s words. Red with errorText
    falling back, and with %1 dropped.
  - smoke.sh checks /mnt/file is 1 MiB. Red with dd writing nothing.
  - check-report.sh checks $before is 64 hex characters. Red with
    disk_sum's drop_caches step failing.
  **Layman:** Some automatic checks would still say all is well if the thing they check broke; make each one catch its break.
  Kind: review-fix.
  Source: review-tests-2026-11-18.
  Lanes: Engine link, Drives, Map view, Screens, Messages, Build.

- ✅ [DFR-0077] **Window: the engine link ends each run once, promptly and truthfully.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  - `src/app/engine/EngineProcess.cpp:110`, `src/app/model/Report.cpp:124`
    and `Report.h:41` — with the engine missing, ended() is queued
    (QTimer::singleShot(0)) while isRunning() (m_process.state()) is
    already false, so a second start() resets m_outcome and queues a
    second ended(); Report::run's guard lets it through and failed() fires
    twice. EngineProcess.h:48 and Report.h:37-39 promise "Ignored while a
    run is going"; DriveList.cpp:25-27 keeps m_listing for this reason.
    Fix: isRunning() stays true until ended() is emitted (m_pendingEnd),
    which fixes Report too. One cause, three findings.
  - `src/app/engine/EngineProcess.h:39` — "Lets go of a running engine"
    reads as instant; ~QProcess (EngineProcess.cpp:59-63) waits up to 30 s
    for a root engine it cannot kill, so closing the window mid-report
    freezes it. A non-root child dies at once (destroyedMidRun, 0.62 s).
    Fix: say so in the header; call m_process.closeWriteChannel() first,
    which becomes cancel with DFR-0023 (§ 4.8).
  - `EngineProcess.cpp:128` — after the decoder flags Malformed or
    VersionMismatch (possible at the first HELLO) the run ends only when
    the engine exits, so a root report walks the whole drive under a
    "running" screen. Fix: once m_decoder.error() is set, end at once;
    kill a probe run, close the write channel on a pkexec run.
  - `EngineProcess.cpp:52` — FailedToStart reads as EngineMissing or
    LauncherMissing, but :107 already found the file present and
    executable, so ENOEXEC, a noexec mount or a bad interpreter says
    "missing" with the file there. Fix: a distinct EngineUnstartable end
    and message when the file exists.
  - `src/app/engine/Records.cpp:58` — fixedSize's switch (58-72) and
    decode's (130-219) are two hand-kept lists of types; a type added to
    decode only gets size 0, the check at :124 passes any payload and
    in.at<> reads past it. Fix: each decode case checks its own fixed
    size, or fixedSize's default is unreachable for a decoded type.
  - `Records.cpp:240-243` — orderAllows' major check never runs: decode
    (119-123) rejects a first HELLO of another major, and a second HELLO
    fails at :238. Fix: keep the one copy that runs.
  Found by review-code lane 03 and review-readability lane 03.
  Priority: 2.
  Shipped 2026-11-18. isRunning() stays true until a queued ended() is
  emitted, so a second start() is ignored and Report fails once. The
  destructor says it waits up to 30 s for a root engine, and closes the
  write channel first. A decoder error ends the run at once: an engine
  run as this user is killed, a launcher run has its write channel
  closed. FailedToStart for an engine start() found present is the new
  RunEnd::EngineUnstartable, with its own message. PayloadReader bounds
  every field and the decoder fails a short payload, so fixedSize's
  hand-kept list is gone; orderAllows' unreachable major check is gone.
  Six new checks in tst_engineprocess, tst_report and tst_messages failed
  before the fix; tst_records' short-payload check, added for the
  restructure, fails with the bound removed. The spec's § 6 line for a
  stream broken mid-run goes in with DFR-0081.
  **Layman:** When the engine is missing, broken or closed mid-run, the window says so once, quickly and in the right words.
  Kind: review-fix.
  Source: code-quality-review-2026-11-18.
  Lanes: Engine link, Records, Report.

- ✅ [DFR-0078] **Engine: probe and report agree, and an odd kernel answer cannot loop or mislead.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  - `src/engine/probe.asm:216-219` — probe_find_mount lets the last
    mountinfo line matching the path decide (found_drive is overwritten
    per match), while judge_line's mount-id check passes only the mount on
    top. If a covered mount's line comes after the top one, report answers
    NOT_A_MOUNT for a drive probe listed, against ADR-0002. Whether
    mountinfo can order them so is unverified. Fix: accept if any
    matching line passes, and take that line's id.
  - `src/engine/extents.asm:153-156` — extents_count's FIEMAP loop has no
    no-progress guard (extents_space has one, :262-268); extents ending at
    or before the requested start, not flagged LAST, repeat for ever.
    Fix: if the new r15 is not past the previous one, return -EIO.
  - `src/engine/main.asm:235-240` — if the second, sending extents_count
    fails part-way, the file counts as skipped, but runs send_run already
    sent (extents.asm sends inside the extent loop) stay on the map. Fix:
    send a file's runs only once all its calls succeed, or state in § 4.6
    that a skipped file's earlier runs may stand.
  - `src/engine/kernel.inc:180` — the comment says fsmap.device is
    new_encode_dev's dev_t unconditionally; linux/fsmap.h makes it a dev_t
    only when fmh_oflags has FMH_OF_DEV_T. Fix: say so, add
    FMH_OF_DEV_T equ 0x1, and have extents_space check fmh_oflags (no
    lane checked whether it does).
  Found by review-code lanes 01 and 02, review-readability lane 01.
  Priority: 2.
  Shipped 2026-11-18. (1) Mountinfo can put a covered mount's line after
  the top one: a drive moved onto a newer mount keeps its older place.
  probe_find_mount now accepts the line judge_line passes and takes its
  id; image_report's moved-drive check failed before (exit 3) and passes.
  (2) extents_count returns -EIO when a FIEMAP call ends no further on.
  (3) Runs cannot be held back in bounded memory, since a full batch
  leaves at once; § 4.6 already says a stage-3 skip may have sent runs,
  and main.asm now points there. (4) kernel.inc says fsmap.device is a
  dev_t only with FMH_OF_DEV_T; extents_space returns -EIO without it.
  (2) and (4) have no red test: only a misbehaving kernel answers so.
  image_report's map checks pass with (4), so ext4 sets the flag here.
  probe's output on this machine is unchanged.
  **Layman:** The drive list and the report always agree on which drives can be read, and a strange answer from the system cannot hang a report.
  Kind: review-fix.
  Source: code-quality-review-2026-11-18.
  Lanes: Probe, Extents, Entry.

- ✅ [DFR-0079] **Map view paints correctly and cheaply at fractional display scaling.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  - `src/app/ui/DriveMapView.cpp:271-273` and `:282-284` — tile() and
    glow() test `deviceIndependentSize() != QSizeF(side, side)`, but
    paintedCell builds `QSize(side, side) * ratio`, which rounds
    (qSaturateRound, qsize.h), so at 125% or 150% the cache misses on
    every call: a QPixmap and QPainter per visible cell per paint, 60 a
    second in a sweep, and a QRadialGradient per glowing cell. Settled by
    the orchestrator from qsize.h. Fix: size as qCeil(side * ratio) and
    cache on a stored (side, ratio) key, not a float compare. (MEDIUM.)
  - `DriveMapView.cpp:20-27` — paintedCell never fills its pixmap, so
    device pixels past side * ratio may be uninitialised. Fix: fill it
    with the map background first.
  - `DriveMapView.cpp:301` — mapKey paints each swatch once at
    key->devicePixelRatioF() when built and never again, so on a screen
    with another scale the swatches are scaled and blur. Not confirmed
    on screen. Fix: repaint them on a device-pixel-ratio change.
  Check on a real display at 150%, counting pixmaps made per paint
  before and after.
  Found by review-code lane 04 and review-readability lane 04.
  Priority: 2.
  Shipped 2026-11-18. Cell and glow pixmaps are sized qCeil(side * ratio)
  and cached on a stored (side, ratio) key; a cell's pixmap is filled with
  the map background before its pattern. Measured offscreen with a
  temporary counter and QT_SCALE_FACTOR: before, at 1.1 and 1.3, 40
  pixmaps per paint (one per cell, every paint), and 4 once at 1, 1.25,
  1.5 (the test font's side times those is whole); after, 4 once at all
  five. The key's swatches paint their pattern at every paint, so they
  stay sharp at any scale. Not checked on a real 150% display.
  **Layman:** On screens set to 125% or 150% size, the drive map stops redrawing every block from scratch, and its key stays sharp.
  Kind: review-fix.
  Source: code-quality-review-2026-11-18.
  Lanes: Map view.

- ✅ [DFR-0080] **Window: numbers and counts read correctly in every language.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  - `src/app/ui/ReportView.cpp:154-160` — `tr("%1 min %2 s").arg(seconds
    / 60)…` uses QString::arg(int), which writes C-locale digits, against
    design.md § Wording and languages ("Numbers … are written by
    QLocale"). Fix: %L1 and %L2, or QLocale().toString.
  - `ReportView.cpp:129` — "Fragmented files found so far: %1" never
    passes the count to tr() as n, so it always says "files", against
    "A sentence with a count uses Qt's plural forms". Fix: pass n and use
    %Ln; add the forms to defragr_en.ts (other languages stay English
    until translated, design.md § Wording and languages).
  - `src/app/model/Report.cpp:49-52` — only the low end is guarded
    ("under 0.1%"); 99.95% or more fragmented but not all shows "100%".
    Fix: show "over 99.9%" when some files are whole and rounding
    reaches 100.
  Found by review-code lane 04 and review-readability lane 03.
  Priority: 3.
  Shipped 2026-11-18. The elapsed time and found count move from ReportView
  into model::elapsedText and model::foundLine, so tst_report can check
  them. Times use QLocale's digits (an Egyptian Arabic locale gave ASCII
  digits before). The count is a plural sentence, "Found 1 fragmented
  file so far."; each language's old label-style translation fits any
  count, so it fills every plural form. A share that rounds to 100% while
  some files are whole reads "over 99.9%" (it read 100% before). The
  moved time strings keep their finished translations; "over %1" is
  English-only until translated.
  **Layman:** Times, counts and percentages read naturally in each language, and 100% shows only when it really is all.
  Kind: review-fix.
  Source: code-quality-review-2026-11-18.
  Lanes: Report view, Report, Languages.

- ✅ [DFR-0081] **Amend DFR-0002 where the code and the protocol's words disagree.**
  Amend docs/specs/DFR-0002-engine-protocol.md (write-spec's
  references/amending.md, then write-doc). Verified 2026-11-18 at 6e9b702.
  (a) § 4.6: REPORT_SUMMARY.extents and .file_blocks are "over those
      files" and "their size_blocks", which could mean fragmented files
      only; main.asm:247-248 sums every measured file. Say which.
  (b) § 4.7 defines INTERNAL as "the engine broke one of its own
      limits", but a failed rt_sigaction also sends it (main.asm:138-146,
      with HELLO.command 0 even when argv[1] names a command; § 4.3 is
      silent), and a write error other than EPIPE exits 9 with no
      FINISHED (records.asm:136-138). Name both in § 4.7 and § 6, or give
      them their own code.
  (c) § 4.4 says "the drive's block size", but HELLO.block_size is
      f_bsize (walk.asm:135) and DRIVE.block_size is f_frsize
      (probe.asm:467); equal on ext4. Say which, or use one.
  (d) Owner ruling 2026-11-18: keep the code (the probe accepts fuseblk
      mounts with a real block device, such as ntfs-3g and exfat-fuse);
      INV-12's Breaks-when names only FUSE mounts with no block device.
  Also open from the same review; settle here: (e) Records.cpp:253
  accepts an Unknown record after REPORT_SUMMARY, citing § 4.10, while
  INV-8 puts the summary immediately before FINISHED. (f) walk_names
  counts a nested mount point's name but not its contents, while INV-14
  compares with find -mindepth 1, which crosses mounts.
  (b) and (c) may be better fixed in code than in words. Whoever works
  this decides which way for each, and records why.
  Found by review-code lanes 01, 02 and 03 (open questions).
  Priority: 3.
  Shipped 2026-11-18. Words only, for all six: (a) extents and file_blocks
  sum every file counted; (b) § 4.3, § 4.7, § 6 and § 10 name a failed
  SIGPIPE setup and a failed write as INTERNAL, one code since each is a
  bug to report; (c) HELLO.block_size is f_bsize and DRIVE's f_frsize,
  which FUSE answers separately; (d) INV-12 per the owner's ruling; (e)
  INV-8 lets an Unknown record follow the summary, for later minors; (f)
  INV-14 counts a nested mount point as one name, tested with find -xdev.
  § 6 gains DFR-0077's "stream breaks a rule mid-run" bullet. Code for (b)
  or (c) could not be shown red: ext4 gives one value for both sizes. The
  failed-write case's missing test is DFR-0089.
  **Layman:** Make the engine's written rulebook say exactly what the engine does in a few corner cases.
  Kind: doc-fix.
  Source: code-quality-review-2026-11-18.
  Lanes: Records, Entry, Walk, Probe.

- ✅ [DFR-0082] **Gate and CI: no check passes without having run.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  Project-owned lines; template text goes to the template item.
  - `scripts/local-ci.sh:103-106` — a green ctest prints only "ok test";
    CTest's "did not run" list stays in .git/local-ci/test.log. Every
    skip is guarded by DEFRAGR_VM_REQUIRED today (run.sh:36 exits 125),
    so latent. Fix: after ctest, grep the log for Not Run and fail when
    DEFRAGR_VM_REQUIRED=1. (LOW, calibrated.)
  - `local-ci.sh:156` — shellcheck selects by extension;
    tests/app/fakes/pkexec-pass/pkexec and pkexec-refuse/pkexec (tracked,
    #!/bin/sh) are never linted. Fix: also take tracked files whose first
    line is a sh or bash shebang.
  - `local-ci.sh:24-25` against `:155-158` — every compiler-free check is
    to run in both modes, but lint comes after the --docs exit (:124).
    Fix: move lint above the DOCS_ONLY exit.
  - `.github/workflows/ci.yml:79` — the NASM cache key omits
    scripts/build-nasm.sh, unlike ci-container.sh:51's image tag, so a
    recipe change restores the old binary; ImageOS names only the OS, so
    :68's "and the image" is wrong. Fix: add
    hashFiles('scripts/build-nasm.sh'); fix the comment, or add
    ImageVersion if a rebuild per image is meant.
  - `scripts/check-links.py:13` — inline links on one line only:
    reference definitions, links split across lines and <angle> targets
    pass unchecked, %20 is not decoded. None in the tree today. Fix:
    parse reference definitions, join a paragraph's lines, decode.
  - `local-ci.sh:55-57` — --docs-glob, --nasm-pin and --ci-packages test
    only ${1:-}, so extra arguments pass despite :20. Fix: check $#.
  - `local-ci.sh:168-169` — jobs sized from cores; local-gate.md § 9 sizes
    from memory per job by peak RSS, and DFR-0062 measured time only.
    Fix: measure peak RSS per job; size from it, or record an override in
    docs/standards/.
  - `scripts/update-translations.sh:19,25` — cd runs before work=$2, so a
    relative --check DIR lands under the repository. Fix: make it
    absolute first.
  - `update-translations.sh:38` — no nullglob; with no .ts files the
    pattern reaches lupdate, which would create defragr_*.ts. Fix:
    nullglob, and fail on none.
  - `scripts/ci-container.sh:70` — warm ignores any argument but
    --refresh, so a typo builds unrefreshed. Fix: refuse others.
  Found by review-code lane 05 and review-readability lane 05.
  Priority: 3.
  Shipped 2026-11-18. All ten. The test step fails on CTest's "did not run"
  list (a skipped test passed plain ctest, exit 0, and fails the gate);
  shellcheck also takes tracked files whose first line runs sh or bash
  (the two stand-in pkexec programs, both clean); lint runs in both modes;
  the print options refuse extra arguments (the old script printed and
  exited 0); jobs are capped by free memory at 640 MiB each, after a
  one-job build peaked at 494 MiB (half the cores still binds here); the
  NASM cache key holds the build recipe's hash and its comment names the
  OS, not the image; check-links.py reads wrapped links, reference
  definitions, angle targets and %-escapes (the old one missed four kinds
  and flagged a valid %20 link); update-translations.sh makes --check's
  folder absolute first and fails on no .ts files; ci-container.sh warm
  refuses anything but --refresh.
  **Layman:** The safety checks run before each upload never report a pass for a check that did not actually run.
  Kind: review-fix.
  Source: code-quality-review-2026-11-18.
  Lanes: Build.

- ✅ [DFR-0083] **Copy the fixed hooks and gate passages in from the template, once claude-config fixes them.**
  Verified 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  .githooks/commit-msg, pre-commit and pre-push are byte-identical to
  ~/.claude/skeleton/files (cmp, 2026-11-18). scripts/local-ci.sh is not,
  but the passages below are template text. Owner ruling 2026-11-18: the
  claude-config session fixes the template first, then copy it in, as
  DFR-0055 did.
  - `.githooks/pre-commit:80` — the link check duplicates
    scripts/check-links.py and has drifted: the hook skips docs/ (:70),
    dated lines (:96) and paths ending `-` (:86) and reads code blocks;
    check-links.py does the reverse. Sent by DFR-0055; survived the copy.
  - `pre-commit:85` — :80's capture class has no `:` or `*`, so the http*
    skip matches only relative paths starting "http", and the `*'*'*`
    skip never fires.
  - `pre-commit:86` — `*-) continue` has no known reason (:77-79).
  - `pre-commit:91-92` — existence is tested in the working tree, though
    :66-67 says every read is staged. Fix: git cat-file -e ":$path".
  - `pre-commit:105` — `${#gone}` counts by locale; under LC_ALL=C the
    45-character floor shrinks for non-ASCII text.
  - `.githooks/commit-msg:48` — the rewrites at :46-49 say not which
    subject shapes each repairs.
  - `commit-msg:143` — `_low` (an id), `_lo` and `_hi` (numbers).
  - `scripts/local-ci.sh:94` (template :70) — says a step skips itself
    with need(); need() stops the gate (:80-82).
  - `local-ci.sh:96` (template :72) — one LOG_DIR for every worktree, so
    two gates at once overwrite each other's log.
  - `local-ci.sh:36` (template :33) — "Tool versions are pinned HERE";
    here only NASM is pinned (ci.yml:58's copy is in the build-comments
    item).
  Then: copy the hooks, re-cmp them byte-identical, merge the local-ci.sh
  passages.
  Found by review-readability lane 05 and review-code lane 05.
  Priority: 3.
  Unblocked 2026-11-18: claude-config fixed all ten in the skeleton,
  commit 7746100. Copy .githooks/* in byte for byte and merge the
  template's local-ci.sh passages. Two differ from the fixes above:
  need() did not stop the gate, it let a skipped step end in "all
  checks passed"; need() now records the skip and finish() declares it
  or exits 1. The shared log is fixed by each step writing a temporary
  file and renaming it into place, not by a log per worktree. Links are
  checked with git ls-files --cached, and fenced code is skipped.
  Shipped 2026-11-18. .githooks/commit-msg, pre-commit and pre-push copied
  from the skeleton at 7746100; cmp says each is byte-identical. From the
  template's local-ci.sh: need() records a skip and finish() names it,
  failing the run unless ANTS_GATE_SKIPPED takes it (checked with
  shellcheck hidden from PATH: rc 1, and rc 0 with the skip written to the
  hook's file); run() writes each log to a temporary file and renames it
  into place; the pin comment says "versions, or names where the runner
  supplies the version". The build and test steps sit under one need
  chain, so a missing cmake, nasm or ninja skips both by name.
  **Layman:** The shared project template's safety-check scripts have small faults; once they are fixed there, copy the fixed versions here.
  Kind: review-fix.
  Source: review-readability-2026-11-18.
  Lanes: Build.

- ✅ [DFR-0084] **Tests: no race on a busy machine, and no idle waits.**
  Confirmed by review-tests' orchestrator 2026-11-18 at 6e9b702; do not
  re-review (review-cost.md § 4).
  - `tests/app/tst_engineprocess.cpp:195` — msleep(600) assumes
    chatter.sh's burst (written 0.3 s after HELLO) is in the pipe at
    `delete engine`; under ctest -j it may not be, and the path goes
    untested. Fix: the fake writes a marker file after the burst; poll
    for it without processing events. (MEDIUM.)
  - `tst_engineprocess.cpp:37` — qWait(50) misses a later duplicate
    ended(). Fix: wait for !isRunning(), sendPostedEvents, then count.
  - `tst_engineprocess.cpp:193` — a failed QVERIFY returns before
    `delete engine`; its `exec sleep 30` child holds stderr and CTest
    waits 30 s. Fix: own it with std::unique_ptr or QScopeGuard.
  - `tests/app/tst_drivelist.cpp:179` — qWait(500) for slow-probe.sh's
    drive; if later, the re-entry check proves nothing. Fix: a marker
    file and qWaitFor.
  - `tst_drivelist.cpp:168` — 1.08 s, nearly all slow-probe.sh's sleep 1.
    Fix: shorten the pause once the marker exists.
  - `tests/app/tst_probe.cpp:160` — mountedVolumes() is read after
    initTestCase's probe; an automount between fails a check. Fix: read
    before and after; compare devices present both times.
  - `tests/app/tst_mainwindow.cpp:205` with `:188` — the 1.5 s budget,
    ReportView's 1 s tick and window.sh's sleep 2 must stay in order
    (about 0.5 s slack). Not reproduced in 180 runs; queued because the
    same fix removes :188's 2.2 s. Fix: window.sh holds the run until the
    test releases it (a file); wait with kWaitMs. (LOW.)
  - `tst_mainwindow.cpp:252` — 2.1 s from slow-probe.sh's pause, which
    guards no assertion (each runs before an event is processed). Fix: a
    probe with no pause.
  - `tests/app/tst_drivemap.cpp:161` — a wall-clock `< 500` ms fails
    under load or a sanitizer build. Fix: QBENCHMARK outside the gate, or
    a limit far above any build. (LOW, calibrated.)
  - `tests/image/check-report.sh:118` — `timeout 1 head -c 1 <&3` always
    waits the full second (fd 3 has a writer), after the engine exited.
    Fix: a non-blocking read (dd iflag=nonblock).
  Found by review-tests lanes 1-3.
  Priority: 3.
  Shipped 2026-11-18. Fakes now hold their run until the test releases
  it (fakes/hold.sh, FakeHold.h), replacing fixed sleeps and qWaits:
  destroyedMidRun waits for the burst to be sent (green before with the
  burst never sent), drivelist waits for slow-probe's drive (green
  before with it late), run() waits for !isRunning() then
  sendPostedEvents, the chatter engine is held by unique_ptr (a failed
  check cost 30 s, now 10 s), tst_probe compares devices present both
  before and after, mainwindow's report is held until checked and its
  Refresh test uses a probe with no pause, drivemap's time limit is 2000
  ms (33 ms here), check-report.sh reads the automount pipe without
  waiting. Times: tst_engineprocess 1.65 s to about 0.35 s,
  tst_drivelist 1.31 s to about 0.16 s, tst_mainwindow 6.17 s to 1.9-3.5
  s. Each changed test passed 10 repeats, the whole suite 5 repeats at
  -j 8. Duplicate ended() and the automount race were not reproduced.
  **Layman:** Make the automatic checks reliable when the computer is busy, and stop them sitting idle.
  Kind: review-fix.
  Source: review-tests-2026-11-18.
  Lanes: Engine link, Drives, Screens, Map view, Build.

- ✅ [DFR-0085] **Engine code a stranger can change safely.**
  Confirmed by review-readability's orchestrator 2026-11-18 at 6e9b702;
  do not re-review (review-cost.md § 4).
  - `src/engine/main.asm:156` — the header lists the report stream as an
    order, but runs come from extents_space (:200) before any file and
    batches go out mid-walk. Say FRAGMENTED_FILE, FILE_SKIPPED, MAP_RUNS
    and REPORT_PROGRESS interleave (§ 4.6).
  - `src/engine/CMakeLists.txt:5-6` — "its one home" for NASM 3.02, but
    README.md:58, local-ci.sh:46 and ci.yml:65 repeat it. Say this file
    enforces it and name the copies (DFR-0052's choice stands).
  - `src/engine/probe.asm:388-394`, `:406-412`, `:532-536` — three copies
    of "fs_buf equals this name" (two repe cmpsb, ext4 a 4-byte load).
    One (name, length) helper for all three; the ext4 comment is
    DFR-0052's.
  - `probe.asm:424-439` — judge_line inlines the rdev split, uncommented.
    Move the btrfs source-to-device step into its own function, headed by
    kernel.inc:109-111's formula.
  - `probe.asm:762` — `rotational` returns '0', '1' or -1, not a yes/no.
    Rename (read_rotational_byte).
  - `src/engine/records.asm:261` — map_run_size is a bare `shl ecx, 4` at
    200, 215, 256 and 261 (261 uncommented), and send_batch computes the
    size twice (255-257, 260-262). Derive it once. (MEDIUM.)
  - `src/engine/walk.asm:287` — r12 turns from the entry buffer (:203)
    into a descriptor that reopen takes silently (306-307, 320-321); r15
    from name length to errno (342, 370). Pass reopen its descriptor in
    an argument register, or split the open-and-check part. (MEDIUM.)
  - `walk.asm:3` — says walk calls only sys.asm per design.md, which
    allows sys.asm and records.asm. Say so.
  - `walk.asm:64` — walk_level's comment omits .len (entry bytes read).
    Rename (dents_len) or describe it.
  Found by review-readability lanes 01 and 02.
  Priority: 4.
  Shipped 2026-11-18. All nine. report_run's header says which records
  interleave; CMakeLists says it enforces the NASM floor and names the
  copies; probe's three name compares share fs_type_is; the btrfs
  source-to-device step is device_of_source; rotational is
  read_rotational_byte; records.asm takes map_run_size from the struct
  and send_batch computes it once; walk gives each register one role
  (the path-only fd in rbp, reopen's fd in edi, errno kept in edx by
  close_keeping_rdx); walk's header and dents_len say what they hold.
  Differential: probe output identical with free_blocks masked,
  selftest-records identical, objdump .text identical for extents, main
  and sys, and differs elsewhere only where the code was meant to;
  vm_smoke, image_build and image_report green.
  **Layman:** Tidy the engine's code and notes so the next person to change it is not misled.
  Kind: refactor.
  Source: review-readability-2026-11-18.
  Lanes: Entry, Probe, Records, Walk.

- ✅ [DFR-0086] **Window code a stranger can change safely.**
  Confirmed 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  - `src/app/model/DriveMap.h:13` — CellState's count is a bare 4 at
    DriveMap.cpp:59 (twice) and :82 and DriveMapView.h:94 and :97; a fifth
    state indexes past them silently and mapKey (DriveMapView.cpp:293-298)
    omits it. One count beside the enum, or a static_assert. Also
    DriveMap.cpp:82's local `blocks` shadows DriveMap::blocks(). (MEDIUM.)
  - `src/app/ui/DriveMapView.cpp:35` — the glow's 3-cell reach is
    hand-kept at :35, :177-182, :205 and :283. One constant. (MEDIUM.)
  - `DriveMapView.cpp:39-56` — glow colours (lighter(150), white, alpha
    110, opacity 0.45, lightness 128) and `src/app/ui/MainWindow.cpp:59`'s
    resize(800, 450) are chosen outside Theme, against design.md ("Only
    ui/Theme chooses a colour, symbol or size"). Move them into Theme.
  - `src/app/ui/Theme.h:2` cites design.md § Who may call whom, which does
    not exist; it is § What may depend on what.
  - `src/app/ui/Theme.cpp:57` — says Desktop uses dark high contrast's
    map colours; the constructor (:101-104) never calls coloursOf for
    Desktop. Say the case only covers every value.
  - `src/app/model/Report.h:76` — m_incoming holds only files, beside
    three other m_incoming* members. Rename m_incomingFiles.
  - `src/app/main.cpp:15` — `languages` is unused by name but installs
    the translators, so it must outlive the window. Say so.
  - `src/app/ui/ReportView.cpp:111` — showFound only records. Rename
    (noteFound).
  - `DriveMapView.cpp:217` — sortMap sorts nothing; it fits the map to
    the cells. Rename it and m_sortedFor (fitMapToCells, m_fittedFor).
  - `DriveMapView.cpp:104` — elapsed returns a fraction, not a time.
    Rename (progress).
  - `src/app/CMakeLists.txt:76` — defragr-window is set up at 61-74 and
    again at 89-91. Keep each target's lines together.
  Found by review-readability lanes 03 and 04, review-code lane 04.
  Priority: 4.
  Shipped 2026-11-18. All eleven. model::kCellStates replaces every bare
  4, with static_asserts; the glow's reach is kGlowReach; the glow
  colours and the window's starting size are Theme's (glowHalo,
  glowWash, kGlowWashOpacity, kWindowStartSize); Theme cites § What may
  depend on what and says Desktop's case only covers every Id;
  m_incomingFiles, noteFound, fitMapToCells and m_fittedFor, and
  progress are renamed; main.cpp says why languages must outlive the
  window; each CMake target's lines are together. The reach and the
  swell, counted in cells, stay in DriveMapView. Glow frames
  byte-identical before and after at scale 1 and 1.5 (a one-step alpha
  change was caught); translations unchanged.
  **Layman:** Tidy the window's code and notes so the next person to change it is not misled.
  Kind: refactor.
  Source: review-readability-2026-11-18.
  Lanes: Map view, Theme, Report, Report view, Languages, Build.

- ✅ [DFR-0087] **Build and container scripts: comments that say what the code does.**
  Confirmed 2026-11-18 at 6e9b702; do not re-review (review-cost.md § 4).
  - `.github/workflows/ci.yml:58` — "one commit cannot use two
    versions", but only NASM is pinned; the packages float with the
    runner. Say the list lives in the script and its versions follow the
    runner image. (local-ci.sh:36's template copy is in the template
    item.)
  - `scripts/ci-container.sh:117` with `:118-119` — the copied modules
    folder is also where the container finds the kernel (tests/vm/run.sh:62
    tries $modules/vmlinuz); /boot is not mounted, so a host keeping its
    kernel only in /boot gets "no readable kernel image". Say so.
  - `ci-container.sh:56` — no reason given for defaulting to this
    machine's scratch folder (/tmp is RAM here). One clause, naming
    DEFRAGR_SCRATCH for other machines.
  - `scripts/ci-container/Containerfile:6` — RUNNER_PACKAGES is "tools
    ... the gate uses", but ci-container.sh:28 says the image build uses
    some (curl, xz-utils, ca-certificates). Match it.
  - `Containerfile:19` — the DL3008 ignore explains word splitting, not
    why versions float: the image copies a runner that updates weekly
    (ci-container.sh:18-20). Say so.
  Found by review-readability lane 05.
  Priority: 4.
  Shipped 2026-11-18. ci.yml says the package list lives in the script and
  the versions follow the runner image, with only NASM pinned;
  ci-container.sh says the copied modules folder is also where run.sh
  finds the kernel, so a host keeping it only in /boot fails, and why the
  scratch default is this machine's, naming DEFRAGR_SCRATCH; the
  Containerfile's RUNNER_PACKAGES line matches ci-container.sh's, and its
  DL3008 note says versions float because the runner updates weekly.
  local-ci.sh's pin comment went in with DFR-0083.
  **Layman:** Correct notes in the build scripts that would send the next person the wrong way.
  Kind: doc-fix.
  Source: review-readability-2026-11-18.
  Lanes: Build.

- ✅ [DFR-0088] **Map view per-frame costs: measure, then change only what wins.**
  None measured (review-code --performance lane P2, 2026-11-18). Measure
  first (review-cost.md § 5): perf record -g on the window during a sweep,
  heaptrack for allocation counts only, and a painted-cells counter. Keep
  a change only where the numbers show a win.
  - `src/app/ui/DriveMapView.cpp:112` (and glowAt) — a QEasingCurve is
    built per cell per frame. Change: static curves, or inline
    1 − (1 − x)³.
  - `DriveMapView.cpp:188` with `src/app/ui/Theme.cpp:161` — cellRect
    calls Theme::cellSide() about eight times, each building a
    QFontMetrics; tile() and glow() call it again. Change: side, pitch and
    columns once per paint, or the side stored in Theme (m_body is fixed
    after construction).
  - `DriveMapView.cpp:241` — frame() repaints the whole widget, though
    only the arriving band (about a quarter) and its glow margin change.
    Change: update only the moving rows; measure on a real compositor
    (DFR-0061's rig ran offscreen).
  - `DriveMapView.cpp:214` — each capacity change while dragging reruns
    DriveMap::cells over every run, and cells() calls blocks(), walking
    them again. Change: merge a burst of resizes with a short single-shot
    timer; keep blocks() up to date in DriveMap::add. Time sortMap on a
    large MAP_RUNS stream.
  - `DriveMapView.cpp:228` — ReportView::start calls showMap(nullptr),
    which runs about 156 empty 16 ms wakeups. Change: skip moveUntil when
    there are no cells. (LOW.)
  Found by review-code --performance lane P2; Theme.cpp:161 also by
  review-readability lane 04.
  Priority: 5 — measure before changing.
  Shipped 2026-11-18. Measured off screen at 1880×900 (9,936 cells),
  RelWithDebInfo, on a fake clock, paired against HEAD. perf could not
  open events in the session's sandbox, so each suspect was timed alone,
  then each change was timed in and out.
  - Kept: Theme stores the cell side when built. The calls to cellSide()
    took about 2 ms of a 3.4 ms frame.
  - Kept: the easing written out, equal to QEasingCurve's at every point
    tried. Allocations per frame fell from about 10,000 to 5.
  - Kept: showMap(nullptr) starts no frame timer. Paints in the next 3 s
    fell from 158 to 1. noMapShowsNothing locks it; it failed before.
  - Together, the median frame fell from 5.3–8.0 ms to 2.5–3.3 ms in a
    sweep, and from 3.7–6.6 ms to 1.2–1.9 ms while four cells change.
  - Deferred to DFR-0018: repainting only the moving rows. It made a
    four-cell change frame about 20 times cheaper and a sweep frame about
    28% cheaper, but nothing in the window calls changeCells yet, and
    tests that grab() the view repaint all of it, so cannot see a stale
    row.
  - Dropped: merging a burst of resizes. A refit (fitMapToCells, once
    sortMap) of a 2^32-block map of a million runs took about 2–3 ms,
    within one frame. DriveMap now tallies into bins and stores its end,
    so cells() walks no runs and blocks() walks nothing.
  **Layman:** Check whether the drive map's animation wastes computer power, and trim only what measurably helps.
  Kind: perf.
  Source: code-quality-review-2026-11-18.
  Lanes: Map view, Theme.

- ✅ [DFR-0090] **Build optimised by default when no build type is given.**
  Nothing in the tree sets CMAKE_BUILD_TYPE (searched 2026-11-18), so a
  plain `cmake -S . -B build` compiles with no optimisation. DFR-0088's
  timings used RelWithDebInfo; the default build is slower than what was
  measured. Change: in the top CMakeLists.txt, default an unset build type
  to RelWithDebInfo on single-config generators, leaving a packager's
  choice alone. Decide whether local-ci.sh's build/ci keeps its own type.
  Priority: 4.
  **Layman:** Make a copy built from source run at full speed without the builder having to ask for it.
  Kind: fix.
  Source: dfr-0088-session-2026-11-18.
  Lanes: Build.

- ✅ [DFR-0015] **Release 0.2.0.**
  **Layman:** Publishes the version that shows the drive map.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0010, DFR-0011, DFR-0012, DFR-0013, DFR-0014.

- ✅ [DFR-0044] **Report re-runs the drive rules before walking, as ADR-0002 says.**
  src/engine/main.asm report_run calls probe_find_mount, which accepts any
  line of /proc/self/mountinfo, then walks it as root. ADR-0002 says the
  engine re-runs the same rules at the start of every report. So /proc,
  /sys, tmpfs, NFS and FUSE mounts are accepted, and a FUSE or dead NFS
  mount can hang the root engine where cancel cannot reach. Fix: apply
  probe's is-a-drive test to the matched mount and finish with its reason.
  The owner chose fixing the engine over amending the ADR (2026-11-16).
  Found by review-code lanes 01 and 02 independently.
  Shipped 2026-11-17. probe.asm's drive test is now judge_line, which
  probe_line and probe_find_mount both call; the report judges the top
  mount at the path. A mount that is not a drive finishes NOT_A_MOUNT,
  not REFUSED: the window already says "not the top of a drive", and the
  protocol is unchanged. The device-listed-once rule is not applied; a
  second mount of a drive walks the same drive. Spec DFR-0002 § 4.7 and
  INV-12 amended. image_report's tmpfs check failed before the fix
  (exit 0) and passes after; probe's output is unchanged apart from free
  space.
  **Layman:** A report refuses anything that is not a real drive, such as system areas or a network drive, instead of walking it with admin rights.
  Kind: fix.
  Source: review-code-2026-11-16.
  Lanes: Entry, Probe.

- ✅ [DFR-0045] **Walk checks before it opens and bounds before it reads.**
  - walk.asm:307-318 opens a folder, then checks its mount: opening a
    nested automount triggers it, and a dead NFS mount can hang. Candidate:
    openat2 RESOLVE_NO_XDEV; whether it acts before an automount fires is
    unsettled and needs a test.
  - walk.asm:281-296 opens a name, then checks S_IFREG: a device node
    renamed over a listed name is opened by root (a tape rewinds on close).
    Candidate: O_PATH|O_NOFOLLOW, fstatx, then reopen /proc/self/fd/N.
  - walk.asm:210 reads reclen and type before bounding pos against len
    (asm.md "Bound before you index"). A correct kernel never triggers it.
  Found by review-code lane 02 (M3, L2, L3).
  Shipped 2026-11-17. Each name is opened O_PATH with openat2
  RESOLVE_NO_XDEV, checked by statx, then reopened through
  /proc/self/fd; each entry is bounded before its reclen is read.
  Measured on Linux 7.3.3 in the test machine: RESOLVE_NO_XDEV
  refuses an autofs point with EXDEV and does not fire it; the old
  DT_UNKNOWN statx did. image_report locks it: a tmpfs and an
  autofs point inside the drive, the stream unchanged, no wake-up.
  Not tested: the device swap race and a malformed getdents buffer.
  **Layman:** The engine checks what a name is before opening it, so it never opens a device or wakes a network drive by accident.
  Kind: fix.
  Source: review-code-2026-11-16.
  Lanes: Walk.

- ✅ [DFR-0046] **Probe survives stacked mounts, a failed statfs and odd sources.**
  - probe.asm:400-408,432-434 stats by path, so a mount hidden under
    another gets the top one's facts.
  - probe.asm:434-436: one failed statfs ends the whole probe.
  - probe.asm:465-466 stores the block size's low 32 bits unchecked;
    walk.asm:119-124 rejects a value above 32 bits.
  - probe.asm:371-381 trusts any major-0 mount's source as a device path:
    a FUSE mount with fsname=/dev/sdb1 can hide the real ext4 drive from
    the list. Follow the source only for btrfs.
  Found by review-code lane 02 (M2, M4, L1) and review-readability lane 2.
  Shipped 2026-11-17. judge_line opens the mount point O_PATH and
  requires statx's mount id to be the line's own, then takes fstatfs
  from that fd; a failed statfs, or a block size of 0 or above 32
  bits, leaves the mount out. A major-0 source is followed only for
  btrfs. probe now needs Linux 5.8, as report already did.
  image_report locks it: two loop drives stacked on one folder list
  the top one once; a tmpfs named after a loop drive no longer hides
  it. Probe output on this PC (btrfs, ext4, vfat) is byte-identical
  to the previous engine's, free space masked.
  Not tested: a failing statfs and an oversized block size.
  **Layman:** The drive list stays complete and correct when one drive misbehaves or two are mounted on the same folder.
  Kind: fix.
  Source: review-code-2026-11-16.
  Lanes: Probe.

- ✅ [DFR-0047] **Window shows names from a drive as plain text, never rich text.**
  QLabel guesses rich text. ReportView.cpp:72 and :104-106,
  MainWindow.cpp:106-107 and DriveList.cpp:76-77 show names any local user
  can choose. Qt::mightBeRichText is true for "Latest: dir/<img src=x>"
  (PySide6 probe on this host, 2026-11-16). Fix: Qt::PlainText on every
  label that shows drive or file names. Found by review-code lane 03.
  Shipped 2026-11-17. ReportView's heading and latest-file labels and
  MainWindow's note are Qt::PlainText. DriveList's tooltip is escaped
  inside <p>, since the model stays Qt Core only. tst_drivelist
  tooltipShowsNamesAsText failed before (the name read as an image) and
  passes after. The labels have no automatic test until DFR-0063.
  **Layman:** A file whose name looks like web code is shown as its name, not drawn as a picture or link.
  Kind: security.
  Source: review-code-2026-11-16.
  Lanes: Report view, Screens, Drives.

- ✅ [DFR-0048] **Window keeps a mount point's non-UTF-8 bytes intact.**
  Report.cpp:111 passes the mount point through QFile::decodeName, which
  turns invalid UTF-8 into U+FFFD: /mnt/t\xff becomes /mnt/t\xef\xbf\xbd
  (Qt probe, 2026-11-16). The engine is then asked about a path that does
  not exist. Keep the raw bytes from the DRIVE record. Found by review-code
  lane 03.
  Shipped 2026-11-17. Keeping the raw bytes could not work: QProcess
  takes text arguments, so a byte that is not UTF-8 is lost before the
  engine starts. The owner chose to escape it (2026-11-17). The window
  sends the mount point escaped as mountinfo writes it
  (EngineProcess::pathArgument), and report decodes it with probe's
  unescape before any check. Spec DFR-0002 § 4.3 amended; INV-15 added.
  image_report's bind mount at a name holding 0xFF, a space and a
  backslash exited NOT_A_MOUNT before and OK after, with the first
  report's stream; tst_engineprocess pathArgument failed against plain
  decoding and passes after.
  **Layman:** A drive whose folder name has unusual characters can still be reported on.
  Kind: fix.
  Source: review-code-2026-11-16.
  Lanes: Report.

- ✅ [DFR-0049] **True, plain messages when the engine cannot start or list drives.**
  - EngineProcess.cpp:51: when pkexec itself is missing, the end is
    AdminRefused, and the user is told to "enter your password when
    asked", though no prompt can appear.
  - MainWindow.cpp:84 advises trying again, but the drive list has no way
    to refresh.
  - MainWindow.cpp:34 shows no "listing drives" note while the probe runs.
  Found by review-code lane 03 and review-readability lane 3.
  Shipped 2026-11-17. A pkexec that cannot start is its own ending,
  RunEnd::LauncherMissing, whose words name pkexec and ask for no
  password; spec DFR-0002 § 6 amended. A "Refresh drives" button runs the
  probe again, greyed out with "Listing drives…" while it runs, and the
  report advice that sent the user to the list names it. DriveList's
  refresh is ignored while a probe runs (moved here from DFR-0051, since
  the button reaches it). tst_engineprocess launcherMissing,
  tst_messages missingLauncherIsNamed and listAdviceNamesTheRefreshButton,
  and tst_drivelist refreshWhileListingIsIgnored failed before and pass
  after; an offscreen run of the real window showed both states.
  **Layman:** When something stops the engine, the window says what really happened and offers a way to try again.
  Kind: fix.
  Source: review-code-2026-11-16.
  Lanes: Engine link, Messages, Screens.

- ✅ [DFR-0050] **All user-facing wording lives in model/Messages.**
  docs/design.md says only model/Messages holds user-facing text about an
  engine result. MainWindow.cpp:79-81 ("Defrag now" comes later), :100
  (the drive's verdict reason) and :107 (no drives found) live elsewhere.
  The owner chose to move all three (2026-11-16). Messages.h:2's "only
  home" comment is then true. Found by review-readability lane 3 and
  review-code lane 03.
  Shipped 2026-11-17. messages::defragAllowed, noDrives and
  defragComesLater hold them. An offscreen run of the real window, with
  a one-drive and a no-drive probe and the Defrag now dialog, printed
  the same text before and after.
  **Layman:** Every sentence the window shows lives in one place, so it is easy to check and change. Nothing changes on screen.
  Kind: refactor.
  Source: review-code-2026-11-16.
  Lanes: Messages, Screens.

- ✅ [DFR-0051] **Window small fixes from the first full review.**
  - EngineProcess.cpp:117-131 does not check HELLO.command.
  - Messages.cpp:16: the strerror fallback.
  - Report.cpp:120: m_incoming has no cap.
  - Records.h:153 takeRecords() lacks [[nodiscard]].
  - MainWindow.cpp:27-32 and ReportView.cpp:43-47 set up a read-only table
    twice, and the copies differ.
  - Records.cpp:129 fills Drive and the other records by position.
  Found by review-code lane 03 and review-readability lane 3.
  Shipped 2026-11-17. A HELLO for another command ends the run Broken,
  except command 0 with USAGE (§ 4.3). An unnamed errno, or one of 0 or
  below, reads "no details given" (strerrorname_np). Records are filled
  by field name. ui/ReadOnlyTable.h holds the shared table set-up; the
  drive list adds single selection. m_incoming keeps no cap: the owner
  chose every file listed (2026-11-17). tst_engineprocess and
  tst_messages failed on the first two before their fixes.
  **Layman:** Small robustness fixes in the window that a user should never notice.
  Kind: fix.
  Source: review-code-2026-11-16.
  Lanes: Drives, Engine link, Records, Report, Screens.

- ✅ [DFR-0052] **Engine tidy-ups from the first full readability review.**
  - The NASM version floor is copied into eight places (main.asm:5 and
    others); give it one home.
  - sys.asm:4 says only rcx and r11 are clobbered; two wrappers clobber
    more.
  - records.asm:28-36: the self-test data has no comment.
  - walk.asm:45 WALK_DEPTH_MAX is typed in, though derived from STRING_MAX.
  - walk.asm:12 "which may" hides O_NOATIME's EPERM rule.
  - probe.asm:138 "as probe sends it" overstates what is accepted.
  - walk.asm:418: statx_of's comment misses the empty-name use.
  - probe.asm:443 compares fs_ext4 with one 4-byte load.
  - kernel.inc:49 STDIN_FD is unused.
  Found by review-readability lanes 1 and 2.
  Shipped 2026-11-17. The floor's one home is src/engine/CMakeLists.txt,
  which stops configure on an older NASM. probe.asm's comment now says a
  later mount of a listed drive passes too. The engine's code and data
  came out byte for byte the same; only six STDIN_FD symbols left.
  **Layman:** Small changes so the next person reading the engine is not misled.
  Kind: refactor.
  Source: review-code-2026-11-16.
  Lanes: Entry, System calls, Records, Walk, Probe.

- ✅ [DFR-0053] **Find whether the engine's output can be non-blocking.**
  records.asm:119-128 ends the run as INTERNAL on EAGAIN. Whether stdout
  can reach the engine non-blocking through QProcess and pkexec was not
  settled: check fd 1's flags during a real run, or write a test.
  Found by review-code lane 01 (LOW, unsettled).
  Done 2026-11-18: settled by measurement. A scratch Qt program started
  a child with QProcess, as EngineProcess does, and the child's
  /proc/self/fdinfo/1 read "flags: 01", O_WRONLY with no O_NONBLOCK. So
  under the window the engine's stdout blocks and EAGAIN cannot occur; no
  code change. pkexec sits between them in a report and was not measured
  (it needs the admin password); it runs the engine on the same open pipe.
  records.asm's rec_send comment now says why EAGAIN is not retried.
  **Layman:** Check one rare case where a report could stop with an internal error for no real reason.
  Kind: investigate.
  Source: review-code-2026-11-16.
  Lanes: Records.

- ✅ [DFR-0054] **The gate fails when a check's tool is missing.**
  - local-ci.sh:106,145,166: `need X && run ...` under set -e drops a check
    whose tool is missing, and the gate still prints "all checks passed".
    A bash probe confirmed a failed left side of && does not stop it.
  - local-ci.sh:186's pass line is matched exactly by ci-container.sh:131,
    with no comment saying so.
  - local-ci.sh:13-16 lists three modes; the parser takes five, and a typo
    such as --doc is silently accepted.
  - check-links.py:35 resolves a link starting with / from the filesystem
    root; check-links.py:18,26 (two LOWs in message 1).
  - ci-container.sh:146 prints its header as help; nothing says so.
  Found by review-code lane 04 and review-readability lane 4.
  Shipped 2026-11-17. A missing python3 or shellcheck now stops the gate:
  with python3 off PATH, --docs printed "passed" and exited 0 before, and
  exits 1 after. The usage lists all five modes, and anything else exits 2
  with the usage. check-links.py lists files with -z, reads a / link from
  the repository root, and closes a code block only on its own marker; a
  scratch repository failed all three before and passes after. The pass
  line and the help text carry comments.
  **Layman:** The safety check before a push can no longer report a pass when it quietly skipped a check.
  Kind: fix.
  Source: review-code-2026-11-16.
  Lanes: Build.

- ✅ [DFR-0055] **Copy the fixed git hooks in from the project template.**
  .githooks/commit-msg, pre-commit, pre-push and scripts/setup-hooks.sh are
  byte-identical to ~/.claude/skeleton/files (diff -q, 2026-11-16).
  local-ci.sh:27-29 and :58-67 and ci.yml:24 carry template text. Their
  defects went to the claude-config session (owner's choice, 2026-11-16):
  the pre-commit link checker drifted from check-links.py and checks the
  working tree, a predictable /tmp name, advice to use --no-verify, word
  splitting on spaces, a dated-line exemption matching any line, three
  copies of one pipeline, and a wrong comment on how the push hook reads
  DOCS_GLOB. Do this once claude-config reports the template fixed.
  Done 2026-11-18, after claude-config fixed the template (~/.claude
  a0a7de4, its message 2204). .githooks/commit-msg and pre-commit are
  copied in; pre-push and setup-hooks.sh were unchanged, and all four are
  byte-identical to ~/.claude/skeleton/files again. local-ci.sh's and
  ci.yml's template comments take the same fix's wording.
  Review 2026-11-18: two defects this item sent remain after the copy.
  The hook's link check still differs from check-links.py
  (pre-commit:80), and still tests existence in the working tree
  (pre-commit:91). Carried by DFR-0083.
  **Layman:** Once the shared template's hook defects are fixed, copy the fixed files here so the two stay identical.
  Kind: chore.
  Source: review-code-2026-11-16.
  Lanes: Build.

- ✅ [DFR-0056] **Settle the protocol spec's open questions in one amendment.**
  In docs/specs/DFR-0002-engine-protocol.md:
  (a) does a file whose FIEMAP failed count in REPORT_SUMMARY.files?
  (b) which HELLO.command does a known command with extra arguments send?
  (c) is an inline-data file one block or zero in size_blocks?
  (d) total_blocks is statfs f_blocks, counted in f_frsize, but is paired
      with f_bsize (§ 4.5); sizes go wrong where the two differ.
  (e) a HELLO from another major version with a short payload: version
      mismatch or malformed?
  (f) should snap squashfs loop mounts be listed as drives?
  design.md: § Safety says walk refuses a different device (code compares
  mount ids); § What may depend on what says nothing outside the drive is
  read (probe stats /dev sources). The code is deliberate in both.
  Found by review-code lanes 01-03; each has a recommendation there.
  2026-11-17: design.md § Safety's device sentence is fixed by
  DFR-0045 (now openat2 with RESOLVE_NO_XDEV). The /dev half of
  § What may depend on what is still open.
  Done 2026-11-18, one amendment of docs/specs/DFR-0002-engine-protocol.md:
  (a) REPORT_SUMMARY.files counts files whose extents FIEMAP gave; a
  failed FIEMAP is in skipped only (code kept). (b) A known command with
  wrong arguments sends its own HELLO.command (code kept). (c) size_blocks
  rounds each extent up; an inline-data file counts 1 (code kept). (d)
  DRIVE.block_size is now f_frsize, the unit of f_blocks (probe.asm
  changed). (e) HELLO's major is read before its length, so a short HELLO
  from another major is a version mismatch (Records.cpp changed, INV-7
  extended, seen red first). (f) Snap squashfs mounts are no drive, the
  owner's choice; the image test mounts one and checks probe leaves it out
  and report refuses it (seen red first). design.md now names probe's
  status read of a btrfs source under /dev.
  **Layman:** Decide six small rules the engine-window protocol leaves open, and correct two stale lines in the design.
  Kind: doc.
  Source: review-code-2026-11-16.
  Lanes: Records.

- ✅ [DFR-0057] **Write DeFragR's trust boundaries into SECURITY.md.**
  SECURITY.md's trust-boundary list is still empty. The review recommends
  recording, as accepted by design: a root report sends the user's window
  the paths inside folders that user cannot read; the short mount-id reuse
  window between probe_find_mount and walk_start (reaching it already needs
  admin authentication); and that names from a drive are untrusted input
  (see the plain-text item). Confirm each with the owner while writing it.
  Found by review-code lanes 01 and 02.
  Done 2026-11-18: SECURITY.md names the seven crossings with their
  checks, what is not defended, and two things accepted by design, each
  confirmed by the owner that day: a report shows names from folders the
  user cannot open, and the gap between finding a mount and walking it.
  Private vulnerability reporting is switched on for the GitHub repo.
  **Layman:** Write down what the admin-rights engine trusts and what it deliberately allows, so later changes keep to it.
  Kind: doc.
  Source: review-code-2026-11-16.
  Lanes: Entry.

- ✅ [DFR-0058] **tst_engineprocess stops waiting 30 s for its fake engine.**
  tests/app/fakes/chatter.sh:15 ends in `sleep 30`, and ~QProcess waits
  up to 30 s for a running engine (EngineProcess.cpp:58). So
  destroyedMidRun waits out the fake: 30.85 s of the gate's 63.95 s test
  step (.git/local-ci/test.log, 2026-11-16). Make the fake exit on its
  closed pipe or SIGTERM, keeping what the test locks.
  Found by review-code --performance (orchestrator, measured).
  Shipped 2026-11-17. The cause was not ~QProcess: the test ends in
  0.62 s. kill() stopped the fake's shell, but its child `sleep 30` held
  the inherited standard error open, and CTest waited for it. chatter.sh
  now ends in `exec sleep 30`. Measured: tst_engineprocess 1.17 s in the
  gate, test step 51.34 s; with the destructor's disconnect removed the
  test still fails (batchesWhileDestroying 1).
  **Layman:** Cut about half a minute from every pre-push check.
  Kind: test.
  Source: review-code-perf-2026-11-16.
  Lanes: Engine link.

- ✅ [DFR-0059] **Run the gate's tests in parallel, if it measures faster.**
  local-ci.sh:183 runs ctest with no -j, so the three test-machine tests
  run one after another. Serial total 63.95 s, longest test 30.85 s
  (2026-11-16), so -j saves at most about 33 s, less once the 30 s wait
  above is fixed, and less if image tests contend on the spinning drive.
  Measure ctest -j against serial, alternating, same nice/ionice. DFR-0037
  declined -j before the VM tests existed. Do after the tst_engineprocess
  item. Found by review-code --performance lane 03.
  Done 2026-11-18: measured build/ci's ctest serial against -j4, alternating
  three times each at nice 10 and idle I/O on 12 cores: serial 13.0 to
  14.2 s, -j4 3.9 to 5.0 s, all green. local-ci.sh now runs ctest with
  -j "$JOBS", the build's job count (half the cores locally, all in CI).
  **Layman:** Try running the pre-push tests side by side, and keep it only if it is really quicker.
  Kind: perf.
  Source: review-code-perf-2026-11-16.
  Lanes: Build.

- ✅ [DFR-0060] **Measure a cold report's disk pattern before reordering the walk.**
  Three candidates, none a win until measured:
  - walk.asm:284 opens files in getdents64 order, which on an indexed
    ext4 folder follows name hashes, not inode numbers: random seeks into
    the inode table on a cold cache. Change: sort each batch by d_ino.
  - main.asm:196: one thread, one blocking call at a time, so a spinning
    disk has one request queued. Measure iostat -x aqu-sz.
  - walk.asm:46 DENTS_SIZE 4096: about 100 names per getdents64; try 32 KiB.
  Measure with GNU time -v on report /mnt/Games after dropping caches,
  plus blktrace/btt, alternating old and new on a quiet machine. Retake
  after DFR-0023 sets idle I/O priority. Found by review-code
  --performance lane 01.
  Done 2026-11-18, one cold report on /mnt/Games (caches dropped, as
  root, /usr/bin/time -v; iostat -x every 5 s; blktrace not installed):
  19 min 56 s wall, 6 s user, 821.6 s system, 69% of one CPU, result OK.
  3,227,679 files, 3,350,180 extents, 30,595 fragmented. sdb averaged
  40% busy: about 52% in the first 7 min, about 10% from 7 to 15 min,
  about 65% with up to 2,000 reads/s in the last 5 min. So the run is
  mostly kernel CPU, not seeks, and the three walk-order candidates are
  not where its time goes; none is changed. Which call costs it is
  DFR-0072.
  **Layman:** Measure where a report on a big spinning drive spends its time, before trying to make it faster.
  Kind: investigate.
  Source: review-code-perf-2026-11-16.
  Lanes: Walk, Entry.

- ✅ [DFR-0061] **Report view: cheaper live updates and column sizing.**
  - ReportView.cpp:105 updates the "latest file" line once per fragmented
    file (up to about 30,600 on /mnt/Games): elide, two labels, a tooltip,
    a relayout. Paint it from the existing 1 s tick.
  - ReportView.cpp:46 ResizeToContents sizes columns from rows, calling
    Report::data per cell. Size once after load, or bound the precision.
    Qt's resizeContentsPrecision default is unverified.
  - Report.cpp:190 decodes a path each time a File cell is drawn.
  Measure on a replayed 30k-row report first. Found by review-code
  --performance lane 02.
  Done 2026-11-18, measured offscreen with a scratch rig: the real
  ReportView fed a generated report of 30,600 fragmented files through a
  fake engine and launcher. All at once: the run took about 65 ms, the
  event loop never stalled past 14 ms, and the finished table's first
  paint took 4 to 8 ms, so column sizing and path decoding stay as they
  are. One record per read (3,000 files, 2 ms apart): the window's CPU
  time was 1,420 to 1,476 ms; with the found count and latest-file lines
  painted from the 1 s tick instead of on every find, 224 to 253 ms.
  That change is in.
  **Layman:** Keep the window smooth while a report on a big drive streams in thousands of results.
  Kind: perf.
  Source: review-code-perf-2026-11-16.
  Lanes: Report view, Report.

- ✅ [DFR-0062] **Gate and container: smaller build costs.**
  - local-ci.sh:164 builds a fresh tree every push; median 4.7 s against
    DFR-0037's 1.7 s warm-ccache figure; NASM, moc and links are never
    cached. Measure with ccache -z/-s and .ninja_log first.
  - ci-container.sh:119 copies the whole kernel module tree; run.sh needs
    the index and four modules.
  - ci.yml:62: man-db's trigger could be turned off (from the lane's
    memory of Ubuntu; check first).
  Found by review-code --performance lane 03.
  Done 2026-11-18: all three measured and dismissed; no code changed.
  (1) Fresh build/ci: configure 0.8 to 1.3 s; build with a warm ccache
  3.9 to 7.9 s over five runs, 50 of 52 compiles cached. What is left is
  moc, NASM and links, which ccache cannot cache, so the fresh tree stays.
  (2) ci-container.sh's module copy: 218 MB, 0.5 s warm and 2.3 s cold,
  against a 65 s container gate; copying only what run.sh loads would
  save under 1% and add a list to keep in step. (3) man-db: GitHub run
  44185535843's log reads "Not building database; man-db/auto-update is
  not 'true'", so the runner already skips it.
  **Layman:** Small savings in how long the safety checks take to build.
  Kind: perf.
  Source: review-code-perf-2026-11-16.
  Lanes: Build.

- ✅ [DFR-0063] **A test that clicks the window's real buttons.**
  No test drives MainWindow or ReportView; tests/app stops at the models.
  The seams in the drive list, the report-or-defrag choice, the report
  view, progress and the admin-rights message were exercised only by the
  owner's manual run and verify-delivery's scratch harness (2026-11-16),
  which compiled the project's own window sources with a scratch main(),
  ran offscreen with a stand-in pkexec on PATH and a replayed engine
  stream. Turn that pattern into a CTest test.
  Include a drive and a file whose names look like markup, such as
  "<img src=x>", and check the labels show them as written (DFR-0047,
  2026-11-17). Cover Refresh drives too: greyed out, with "Listing
  drives…", while a probe runs (DFR-0049; tests/app/fakes/slow-probe.sh
  pauses mid-probe).
  Done 2026-11-18: tests/app/tst_mainwindow.cpp runs the real MainWindow
  and ReportView offscreen against fakes/window.sh and a stand-in pkexec
  (fakes/pkexec-pass, fakes/pkexec-refuse), and presses their buttons:
  the drive list and each drive's actions, Defrag now's box, a report
  mid-run and at its end, refused admin rights, and Refresh drives greyed
  with "Listing drives…" (slow-probe.sh). Markup-like drive, filesystem
  and file names must show as written. Each check was seen to fail with
  its behaviour broken. MainWindow and ReportView now build as the
  defragr-window library so the test can link them.
  Record corrected 2026-11-18 (review-tests): not every check was seen to
  fail. With the listing branch's m_report->setEnabled(false) removed
  (mutation M2), the View report check at tst_mainwindow.cpp:267 stayed
  green, because no row is selected. Fixed by DFR-0076: the test picks
  a drive before Refresh drives, and with M2 applied it was seen to fail.
  **Layman:** An automatic check that presses the window's own buttons, so a break between the screen and the engine is caught before release.
  Kind: test.
  Source: verify-delivery-2026-11-16.
  Lanes: Screens, Report view.

- ✅ [DFR-0064] **Fill in the README's summary, Install and Usage.**
  README.md line 3 is still "> One line: what this does, for whom.",
  § Install is "(Once there is something to install.)" and § Usage is
  "(Once there is something to use.)". 0.1.0 can be built and used from
  source, so say what it does and how to build and run it.
  Done 2026-11-18: README has its one-line summary, the version with
  links to the changelog and roadmap, Install (build from source; pkexec
  for admin rights) and Usage. The build steps were followed in a clean
  checkout and the window opened listing drives; the report step needs
  the admin password and was not run.
  **Layman:** Replace the README's template placeholders with what DeFragR is and how to try it.
  Kind: doc.
  Source: verify-delivery-2026-11-16.
  Lanes: Build.

- ✅ [DFR-0070] **Hebrew sentences that start with a Latin word read right to left.**
  Found taking the website's screenshots (2026-11-17): with a drive
  selected, the Hebrew drive list's note "DeFragR יכול להפיק דוח..." sat
  on the left, read with its full stop first and DeFragR last. QLabel
  takes a plain text's direction from its first strong letter, not from
  the window. Every Hebrew sentence starting with "DeFragR" does the same.
  Shipped 2026-11-17. In a right-to-left language, Languages' own
  translator puts RIGHT-TO-LEFT MARK in front of a translation that would
  read left to right from its first strong letter. tst_languages failed
  on such a sentence before the fix. A demoreel screenshot of the Hebrew
  drive list shows the note right-aligned, starting with DeFragR.
  **Layman:** In Hebrew, a sentence that starts with the name DeFragR still reads right to left, with its full stop at the end.
  Kind: fix.
  Source: user-request-2026-11-17.
  Lanes: Languages.

- ✅ [DFR-0071] **Make the map's changes more noticeable.**
  Owner, 2026-11-18, after the DFR-0014 demo video: "a bit more flashy,
  more noticeable". A changed cell glows: a soft halo three cells across
  and the cell drawn half as big again and lit, fading out over 900 ms.
  A sweep cell glows the same over 500 ms, so a new map rolls in as a
  bright band over 2 s. The view keeps S10's limit of three flashes a
    second itself: a cell starts at most one change, so one glow, every
    334 ms (DFR-0075). Each state's glow is painted once, so a frame costs
  about what it did without the glow (measured offscreen at 1600x1000).
  Owner approved the glow from the second demo video, 2026-11-18.
  Review 2026-11-18: the three-flashes limit held only because callers
  pace their changes; the view enforces nothing (DriveMapView.cpp:128-151,
  :214). DFR-0075 makes the view enforce it; then this record must say so.
  **Layman:** A changed block lights up and glows over its neighbours before settling, and a new map rolls in as a bright band.
  Kind: enhancement.
  Source: user-request-2026-11-18.
  Lanes: Map view.

- ✅ [DFR-0089] **Test the engine's failed-write exit, and square INV-11 with it.**
  DFR-0002 § 10 says nothing checks § 4.3's failed SIGPIPE setup or § 6's
  failed write. A cheap check: run the engine with standard output on
  /dev/full and expect exit 9 (INTERNAL) with no output, in
  tests/app/tst_records_conformance.cpp; then § 10 names it. INV-11 says
  every engine run writes a stream that decodes with no error, which a run
  whose writes fail cannot do (EPIPE too, before DFR-0081); add one clause
  there. Found while amending DFR-0002 (DFR-0081).
  Priority: 4.
  Shipped 2026-11-18. tst_records_conformance gains
  failedWriteIsInternal (standard output on /dev/full: exit 9) and, since
  the new clause names EPIPE too, closedPipeIsCancelled (a pipe with no
  reader: exit 7, not a SIGPIPE death); both expect nothing on standard
  error. Each failed against a mutated engine in a scratch copy: a failed
  write exiting CANCELLED, one ignored, EPIPE exiting INTERNAL, and
  SIGPIPE left fatal. INV-11 now excepts a run whose write fails; § 10
  names both tests, and the failed SIGPIPE setup keeps nothing, since
  rt_sigaction fails only on a bad argument.
  **Layman:** Add an automatic check that the engine stops cleanly when it cannot write its answer.
  Kind: test.
  Source: dfr-0081-amendment-2026-11-18.
  Lanes: Records, Entry.

## 0.2.1 — Packages on OBS

- ✅ [DFR-0091] **Install rules, a desktop entry and a build name for packages.**
  cmake --install puts defragr and defragr-engine side by side in the
  binary folder (the window runs the engine beside itself), the desktop
  entry in applications, and the icon as SVG and 512 px PNG.
  DEFRAGR_BUILD_ID names the build in the window title where a tarball
  has no git history. The desktop entry's translations stay with DFR-0067.
  A polkit policy, for a friendlier admin prompt, is not part of this.
  **Layman:** DeFragR can be installed like any other app, with a menu entry and icon.
  Kind: package.
  Source: user-request-2026-11-18.
  Lanes: Build, Packaging.

- ✅ [DFR-0092] **Publish DeFragR on OBS for every distribution it builds on.**
  Subproject home:milnet:DeFragR (user's choice 2026-11-18), x86_64
  only. Built from the GitHub tag, so OBS and GitHub carry the same
  version. NASM 3.02 is bundled as a build-only source, since no
  distribution ships it. A distribution lacking Qt 6.4, CMake 3.28 or a
  C++23 compiler is dropped and named here. The recipes are kept in
  packaging/obs/.
  Shipped 2026-11-18, OBS revision 7 built from tag v0.2.1 by _service:
  succeeded on openSUSE Tumbleweed, Leap 16.0 and 15.6 (gcc13-c++),
  Fedora 43, 44 and Rawhide, Debian 13, Ubuntu 24.04 to 26.10, and Arch.
  Dropped: Debian 12 (CMake 3.25). Installed from the published repos and
  run offscreen in Tumbleweed and Ubuntu 24.04 containers (Debian 13 and
  Arch on the trial build). Fedora 42 left out as end-of-life.
  **Layman:** Install DeFragR from a package repository on openSUSE, Fedora, Debian, Ubuntu and Arch.
  Kind: package.
  Source: user-request-2026-11-18.
  Lanes: Packaging.

- ✅ [DFR-0093] **Release 0.2.1.**
  **Layman:** Publishes the version the OBS packages are built from.
  Kind: release.
  Source: user-request-2026-11-18.
  Lanes: Build.

- ✅ [DFR-0094] **tst_probe expects drives the probe is meant to leave out.**
  listsEveryDrive requires every mounted block device that Qt sees to be
  in the probe's list. The probe deliberately leaves some out, such as
  snap disk images (DFR-0056). CI run 44320090496 on the 0.2.1 commit
  failed with /dev/loop0 missing; the 0.2.0 run passed on a runner with
  no loop0 in its log. Fix: compare against the devices the probe's rules
  admit, or exclude what § 4.7's rules exclude, so the test checks the
  rules rather than the runner.
  Done 2026-11-18: mountedBlockDevices skips squashfs, as
  probe.asm's rules do. Seen red first on this PC with
  tests/image/snap.squashfs mounted (/dev/loop0 missing, as on CI), then
  green with it still mounted; local gate green. No CHANGELOG entry: a
  test-only change.
  **Layman:** A test fails on a machine with a snap package mounted, though DeFragR is right to skip it.
  Kind: test.
  Source: ci-run-44320090496-2026-11-18.
  Lanes: Window tests.

- ✅ [DFR-0095] **The release recipe bumps the OBS packaging's version too.**
  packaging/obs/ hard-codes the version in _service (revision and
  versionformat), defragr.spec, PKGBUILD, defragr.dsc and
  debian.changelog; .claude/bump.json lists none of them. Add them to its
  files, add todos for the defragr.changes and debian.changelog entries,
  and a step that uploads to OBS and waits for every repository to read
  succeeded. Reset Arch's pkgrel per release.
  Done 2026-11-18: bump.json bumps the version lines in _service,
  defragr.spec, PKGBUILD and defragr.dsc; todos add the defragr.changes
  and debian.changelog entries and reset pkgrel; post_check is
  scripts/check-version-drift.sh; publish_precheck is
  scripts/obs-publish.sh --check. obs-publish.sh uploads after the tag
  push and waits until every repository's built files carry the version;
  CLAUDE.md § Release names it, since the recipe has no after-push step.
  Rehearsed with commits stubbed out: no-change run at 0.2.1, and a 0.2.0
  copy reaching the commit and the wait. No real upload yet: 0.3.0 is the
  first. No CHANGELOG entry: release tooling only.
  **Layman:** Each new release reaches the OBS packages without anyone remembering to update them by hand.
  Kind: chore.
  Source: dfr-0092-session-2026-11-18.
  Lanes: Packaging, Build.

## 0.3.0 — Single-folder defrag with safety checks

- ✅ [DFR-0016] **CRC32C checksum in the engine.**
  `crc32c.asm` using the CPU's `crc32` instruction, checked against
  published test vectors through a self-test command.
  Done 2026-11-18. crc32c.asm's crc32c_update takes bytes, in pieces
  if need be. The hidden selftest-crc32c checks it against RFC 3720's
  values and the check string (spec § 4.11, INV-18; protocol 1.3).
  INV-18's test went red against an engine that did not invert the
  starting value.
  **Layman:** Gives the engine a fast fingerprint to prove a file is unchanged after it moves.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Checksum.
  Blocked-by: DFR-0003.

- ✅ [DFR-0017] **Engine defrag of one folder with safety checks.**
  `move.asm`: per-file checks, donor file, `fallocate`,
  `EXT4_IOC_MOVE_EXT` in chunks, CRC32C before and after, change
  records. Image test: extents fall, every SHA-256 matches, `e2fsck -fn`
  clean (S7).
  Done 2026-11-18. `defrag <mount-point> <folder>` moves the folder and
  every folder below it; protocol 1.4 adds `FILE_DEFRAGGED`,
  `BLOCKS_MOVED`, stages 6 to 11, `NO_FOLDER`, `CHECKSUM_MISMATCH` and
  reason `OLD_CPU` (`docs/specs/DFR-0002-engine-protocol.md` § 4.12,
  INV-19 to INV-23). The `O_TMPFILE` donor works. Each move's from and
  to runs come from the file's map before and after, as Linux 7.3's
  moved_len leaves out holes. The owner's choice for a CPU without
  SSE4.2 (2026-11-18): every drive is report only, reason `OLD_CPU`;
  `image_defrag` checks it in a test machine with QEMU's `qemu64` CPU.
  INV-19's, INV-21's and the messages' tests went red first; the image
  test went red against an engine whose to runs were its from runs and
  one with no CPU rule.
  **Layman:** The engine safely defrags the files in one folder, checking each file is unchanged.
  Kind: implement.
  Source: design-2026-11-15.
  Lanes: Move, Entry.
  Blocked-by: DFR-0008, DFR-0016.

- ✅ [DFR-0018] **Window defrag of one folder, with the map animating.**
  Pick a folder, confirm, run; `model/RunState` applies change
  records and the map shows blocks turning from fragmented to whole
  (S4). Overlaps DFR-0021 in `model/RunState`.
  Owner request 2026-11-17: make the moves mesmerising. Each chunk
  the engine moves is one short journey: its old cells light up as
  they are read, a trail of small blocks glides along a gentle arc to
  the file's new place and lands cell by cell, the landed cells glow
  and settle as whole, and the old cells fade to free. A soft marker
  rests on the file being worked on. The map trails the engine by
  about a second at most: when files finish faster than it can show,
  small ones settle together, and the final picture always equals
  the drive. For S10, a cell's symbol changes as well as its colour,
  nothing flashes more than three times a second, and turning off
  the desktop's animations turns off the motion. How Qt learns that
  desktop setting is unverified. Repaint only the rows that move:
  DFR-0088 measured a four-cell change frame about 20 times cheaper so.
  A test of it must read what the screen shows, not grab().
  From DFR-0017: a defrag stream holds no map, so the window draws it
  from a report first; each `BLOCKS_MOVED` pairs its from and to blocks
  in order, and the from runs are free once the file's turn ends (spec
  § 4.12). The words for `FILE_SKIPPED` stages 6 to 11 are this item's.
  Done 2026-11-19, to `docs/specs/DFR-0018-window-folder-defrag.md`.
  "Defrag now" asks for a folder and a yes, then `ui/DefragView` shows
  the run. `model/RunState` reads the map with a report unless it holds
  one, so a drive not yet looked at asks for the password twice
  (DFR-0096). Qt 6.12 reads the desktop's animations setting
  (`motionPreference`); before it, the style's animation length of 0
  does. Moves gather into launches at most 150 ms apart; each frame
  repaints only the rows that changed and the small blocks' places, and
  `onlyWhatMovesIsRepainted` reads the screen's own pixels. The marker
  rests where blocks last landed, since no record names a file before
  its moves. Claude's calls, not yet confirmed by the owner: the map is
  used again, the folder dialog lists the drive's folders
  (`docs/design.md`), and the look of the small blocks and the marker.
  Left: `DriveMapView::changeCells` has no caller but tests; a real
  drive has not been defragged from the window (DFR-0034); the new
  sentences are untranslated (DFR-0067).
  Owner, 2026-11-19, after a first real run on /mnt/Emulators: the map
  used again and the folder dialog are confirmed. The look is liked;
  the marker is too hard to find (DFR-0098).
  **Layman:** Choose a folder and watch its blocks join up on the map as it is defragged.
  Kind: feature.
  Source: design-2026-11-15.
  Lanes: Run, Map, Screens.
  Blocked-by: DFR-0014, DFR-0017.

- 📋 [DFR-0097] **A file moved into fewer pieces, not one, looks like new fragmentation.**
  Owner, 2026-11-19, first real run (/mnt/Emulators,
  Sony_PlayStation_3): striped cells appear where files land. By design
  a file moves when its donor is in fewer pieces (DFR-0002 § 4.12 step
  4), and `DriveMap::endTurn(false)` counts what landed as a fragmented
  file's. Not measured: how many files end in one piece, and what
  limits the rest. Owner's choice that day: measure that run when it
  ends, then bring options with numbers; 0.3.0 waits.
  2026-11-19: `image_defrag` failed 2 runs of about 15 while DFR-0099
  was built, once with "deep/big: moved, into fewer pieces": the same
  donor-no-better case DFR-0096's note records. The checker fails what
  § 4.12 allows. Fix it with this item.
  **Layman:** Defragging seems to make new striped blocks on the map; find out how often a file ends whole and why not always.
  Kind: investigate.
  Source: user-test-2026-11-19.
  Lanes: Move, Map.

- ✅ [DFR-0098] **Make the working spot on the map easy to find.**
  Owner, 2026-11-19: hard to see where to look during a run. Owner's
  choice that day: stronger for everyone, with a thicker, brighter,
  pulsing ring and a line above the map naming the file just finished.
  The pulse keeps to three a second at most and stops with motion off.
  A high-visibility setting waits for DFR-0028.
  Done 2026-11-19. The outline is thicker, edged in the opposite shade,
  and goes round the cell last filled and the cells next to it. A
  second outline closes in on it when it moves, at most once a second,
  and not with motion off. The line naming the latest file was already
  there. Left: whether it is easy to find is the owner's to judge.
  **Layman:** A bolder, gently pulsing ring where DeFragR is working, and a line of text naming the file it just finished.
  Kind: ux.
  Source: user-test-2026-11-19.
  Lanes: Map, Screens.

- ✅ [DFR-0099] **Progress bar for a folder defrag.**
  Owner, 2026-11-19: a 234 GB folder ran with no bar and no cancel.
  Owner's choice that day: in 0.3.0, not 0.4.0. The engine says how
  much work the folder holds before it moves anything, a protocol
  amendment. Overlaps DFR-0020 and DFR-0021, which keep whole-drive
  progress, the menu and the status bar. Time remaining stays DFR-0030.
  Done 2026-11-19. The engine walks the folder once to count, then
  sends `DEFRAG_PROGRESS` before the first file and after each file
  that was work (protocol 1.6, `docs/specs/DFR-0002-engine-protocol.md`
  § 4.12, INV-26, INV-27). The defrag page's bar fills by blocks.
  Left: the count is one more walk of the folder, not timed on a real
  drive.
  **Layman:** A bar and 'file X of Y' while a folder is defragged, so a long run shows how far it is.
  Kind: feature.
  Source: user-test-2026-11-19.
  Lanes: Move, Run, Screens.

- ✅ [DFR-0100] **Defrag a folder from the report screen.**
  Owner, 2026-11-19: on the report screen, asked how to defrag a
  folder; the only button is on the drive list. Owner's choice that
  day: before 0.3.0.
  Done 2026-11-19. A loaded report of a drive that can be defragged
  shows "Defrag a folder". The words are untranslated (DFR-0067).
  **Layman:** A 'Defrag a folder' button on the report, so you need not go back to the drive list first.
  Kind: ux.
  Source: user-test-2026-11-19.
  Lanes: Screens.

- 📋 [DFR-0101] **Click a map block to see which files are there.**
  Owner request 2026-11-19, chosen for before 0.3.0. The map holds
  runs by kind and no names, so the engine must answer whose blocks a
  range holds. How is undesigned: `spec-format.md` § 1 decides whether
  it needs a spec.
  **Layman:** Click a block on the map and DeFragR lists the files that live in it.
  Kind: feature.
  Source: user-test-2026-11-19.
  Lanes: Extents, Map, Screens.

- ✅ [DFR-0102] **The dev build gives its engine the label pkexec needs.**
  2026-11-19: a rebuilt `build/dev/bin/defragr-engine` on /mnt/Games
  came out labelled samba_share_t, and pkexec printed "Error executing
  ...: Permission denied" (DFR-0042). `chcon -t bin_t` on it, as the
  file's owner, let it run. Owner's choice that day: before 0.3.0.
  Done 2026-11-19, in `src/engine/CMakeLists.txt`: the engine's build
  ends with `chcon -t bin_t`, and goes on where that fails.
  **Layman:** The test build fixes its own security label after each rebuild, so the window can start the engine.
  Kind: chore.
  Source: user-test-2026-11-19.
  Lanes: Build.

- 📋 [DFR-0019] **Release 0.3.0.**
  **Layman:** Publishes the first version that can defrag, one folder at a time.
  Kind: release.
  Source: design-2026-11-15.
  Lanes: Build.
  Blocked-by: DFR-0015, DFR-0016, DFR-0017, DFR-0018, DFR-0097, DFR-0098, DFR-0099, DFR-0100, DFR-0101, DFR-0102.

- 📋 [DFR-0072] **Find which kernel call takes a big report's system time.**
  DFR-0060's cold report on /mnt/Games took 19 min 56 s, 821 s of it
  system time, while the drive was busy 40% of the time on average and
  about 10% for eight minutes in the middle. Count each call's share
  with strace -c -f on a cold report (needs root), then look first at
  the candidates: GETFSMAP's map (extents_space), FIEMAP read twice per
  file since DFR-0011, and getdents64's small 4 KiB buffer.
  Review 2026-11-18 (review-code --performance lane P1), none measured.
  Changes to try once the counts are in, each kept only if it wins:
  (1) main.asm:237 — a file whose extents all fit one FIEMAP call (256)
  and reached FIEMAP_EXTENT_LAST still has them in fm_buf; send its runs
  from there instead of a second FIEMAP. Count ioctl calls with perf trace
  -s, old and new builds alternating.
  (2) extents.asm:35 — FSMAP_PER_CALL 64 is sized so the image test needs
  a second call (INV-17). If ext4 rebuilds its metadata list on each
  GETFSMAP call, use a large buffer in production and the small one only
  in the test build. Time the map phase with perf stat, 64 against 4096.
  (3) walk.asm:320 — each folder costs openat2(O_PATH), statx, an open
  through /proc/self/fd and a close; one openat2(FOLDER_FLAGS,
  RESOLVE_NO_XDEV) may keep the same guards. Treat ENOTDIR and ELOOP as
  "next"; DT_UNKNOWN keeps today's path.
  (4) main.asm:208 — the walk is one thread and kernel-CPU bound. Several
  walkers feeding one writer is large work, weighed against S8; consider
  it only after perf record -g -a shows no lock bound.
  **Layman:** A report on the big games drive spends most of its 20 minutes with the computer thinking, not the disk reading; find out which step.
  Kind: investigate.
  Source: dfr-0060-measurement-2026-11-18.
  Lanes: Walk, Extents.

- ✅ [DFR-0096] **One password prompt for a defrag that reads its map first.**
  A defrag stream holds no map (DFR-0002 § 4.12), so `model/RunState`
  runs `report`, then `defrag`: two pkexec runs, two prompts, unless the
  window already holds that drive's map
  (`docs/specs/DFR-0018-window-folder-defrag.md` § 3, § 8). Two ways,
  the owner's choice: a polkit policy that keeps the password for a few
  minutes, which a long report outlasts (DFR-0060 measured 19 min 56 s
  cold on /mnt/Games); or the engine sends `MAP_RUNS` at the start of a
  defrag stream, a protocol amendment, with the whole-drive walk before
  every defrag. Neither is tried.
  Done 2026-11-19, the owner's choice that day: fixed before 0.3.0, by
  the engine. `defrag <mount> <folder> map` sends the drive's map, then
  `MAP_DONE`, then defrags, in one run (protocol 1.5,
  `docs/specs/DFR-0002-engine-protocol.md` § 4.12, INV-24, INV-25).
  `model/RunState` asks for it when it holds no map of the drive. The
  folder is checked before the map's walk. Seen once, with the engine
  broken on purpose: `tests/image/defrag-check.cpp` fails a
  `FILE_DEFRAGGED` whose `moved_blocks` is 0, which § 4.12 allows; three
  unbroken runs passed. `tests/fixtures/image-report/report.bin` is
  recorded again at 1.5.
  **Layman:** Defragging a drive DeFragR has not looked at yet asks for the password twice; make it once.
  Kind: ux.
  Source: dfr-0018-build-2026-11-19.
  Lanes: Run, Entry, Packaging.

## 0.4.0 — Whole-drive defrag, menu, status bar and progress

- 📋 [DFR-0020] **Engine whole-drive defrag with progress records.**
  `walk.asm` from the mount root, staying on one filesystem; progress
  records with blocks done and total. Overlaps DFR-0017 in `move.asm`.
  From DFR-0017: `defrag` with the folder "" already walks the whole
  drive. The status bar (S5) needs each file named before its moves,
  which no 1.4 record does.
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
  Owner, 2026-11-17: the theme is chosen from this menu (View, then
  Theme), from Theme::kAll, and the choice is remembered. The themes'
  names are added for translation here, with the menu. Unverified until
  then: that the Breeze style honours a theme's palette, and that a
  desktop colour change while the window is open does not undo it.
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

- 📋 [DFR-0103] **Decide where a defragged file should go.**
  Owner, 2026-11-19: files should be placed so that apps get faster.
  Today the donor's one `fallocate` lets ext4 choose the place
  (`src/engine/move.asm`), and no document asks for more. To weigh: a
  folder's files next to each other, free space gathered into long
  stretches, and which part of a spinning drive is fastest. Nothing is
  measured. DFR-0097's numbers come first. Its release is the owner's
  to choose; filed under 0.4.0 as a guess.
  **Layman:** Today Linux picks where a moved file lands; work out whether DeFragR should choose, so apps load faster.
  Kind: research.
  Source: user-request-2026-11-19.
  Lanes: Move.

- 📋 [DFR-0104] **Defrag several folders in one run.**
  Owner, 2026-11-19: asked how to defrag more than one folder. Today
  each folder is its own run. The drive's top folder already defrags
  the whole drive (DFR-0002 § 4.3). Whether the window queues runs or
  the engine takes several folders is undesigned. Its release is the
  owner's to choose; filed under 0.4.0 as a guess.
  **Layman:** Pick more than one folder and DeFragR works through them one after another.
  Kind: feature.
  Source: user-request-2026-11-19.
  Lanes: Run, Screens.

## 0.5.0 — Pause, cancel and background mode

- 📋 [DFR-0023] **Engine priorities, pause and cancel.**
  `sys.asm`: idle I/O class and lowest CPU priority; `poll` for
  commands between chunks; a closed input means cancel. Image test: a
  cancel mid-file leaves every SHA-256 matching (S8).
  Review 2026-11-16 (review-code lane 02, L4): walk_next returns only
  for a file, a skip, the end or an error, so a large tree of folders,
  links or devices with no regular files is crossed in one call: no
  REPORT_PROGRESS and cancel waits (walk.asm:180-325). Suggested fix:
  return a WALK_FOLDER tick when a folder is entered. Also: report
  timings taken before this item ran at normal priority.
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

- 💭 [DFR-0068] **Choose the window's language in Options.**
  Not asked for: S13 needs only the desktop's language. Today another
  language needs LANGUAGE=<code> on the command line.
  **Layman:** Pick a language other than the desktop's, without changing the desktop.
  Kind: ux.
  Source: user-request-2026-11-17.
  Lanes: Settings, Settings view, Languages.

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

- 📋 [DFR-0065] **Window refuses an engine that root does not own.**
  EngineProcess.cpp:84-95 hands the engine's path to pkexec without
  checking the file is owned by root and not writable by others. Not live
  yet: there is no install layout or polkit policy. Decide with DFR-0033
  where a release installs the engine and whether its polkit policy pins
  the path; then refuse anything else. Found by review-code lane 03.
  2026-11-18: still not live for packages. The OBS packages (DFR-0092)
  install the engine in /usr/bin, owned by root. Only a run from a
  build folder is exposed. The owner's choice: keep it here, with
  DFR-0033.
  **Layman:** Before asking for your password, the window checks the engine file has not been swapped for something else.
  Kind: security.
  Source: review-code-2026-11-16.
  Lanes: Engine link, Packaging.

- 🚧 [DFR-0067] **Translate the window into the S13 languages.**
  Once the wording has settled. One translations/defragr_<lang>.ts per
  S13 language: he, de, it, el, es, pt_BR, pt_PT, zh_CN, zh_TW, ja, ru,
  pl, fr, uk. Also the desktop entry's name and comment. Drafted by
  Claude, not yet checked by native speakers: the README says so and
  invites corrections. Each language is checked on screen, Hebrew right
  to left, including whether sizes such as "200.62 GiB" read correctly
  (DFR-0069 left them).
  Started 2026-11-17, by the owner's choice to translate the words that
  exist now: all fourteen files hold every sentence the window had then,
  and the README's Languages section says they are AI drafts. Each was
  checked on a demoreel screenshot of the drive list; Hebrew mirrors, and
  its sizes read correctly ("GiB 9.02" on screen reads 9.02 GiB right to
  left). Left: sentences added since, the desktop entry once packaging
  makes it, and the report screen, which needs admin rights to show.
  **Layman:** DeFragR speaks Hebrew, German, Italian, Greek, Spanish, Portuguese, Chinese, Japanese, Russian, Polish, French and Ukrainian.
  Kind: feature.
  Source: user-request-2026-11-17.
  Lanes: Languages, Packaging.
  Blocked-by: DFR-0066.

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
