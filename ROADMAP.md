<!-- ants-roadmap-format: 1 -->
<!-- Generated from the Ants Terminal roadmap store. Edit it with roadmap_log; hand edits are discarded by the next write. -->
<!-- ants-roadmap-id-high-water: DFR=70 -->
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

- 📋 [DFR-0041] **Find why the image tests are slow on this PC.**
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
  Owner ruling 2026-11-16: discovery S10 (readable by a partially
  sighted user) binds from 0.2.0, with this theme, not today's screens.
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

- 📋 [DFR-0046] **Probe survives stacked mounts, a failed statfs and odd sources.**
  - probe.asm:400-408,432-434 stats by path, so a mount hidden under
    another gets the top one's facts.
  - probe.asm:434-436: one failed statfs ends the whole probe.
  - probe.asm:465-466 stores the block size's low 32 bits unchecked;
    walk.asm:119-124 rejects a value above 32 bits.
  - probe.asm:371-381 trusts any major-0 mount's source as a device path:
    a FUSE mount with fsname=/dev/sdb1 can hide the real ext4 drive from
    the list. Follow the source only for btrfs.
  Found by review-code lane 02 (M2, M4, L1) and review-readability lane 2.
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

- 📋 [DFR-0053] **Find whether the engine's output can be non-blocking.**
  records.asm:119-128 ends the run as INTERNAL on EAGAIN. Whether stdout
  can reach the engine non-blocking through QProcess and pkexec was not
  settled: check fd 1's flags during a real run, or write a test.
  Found by review-code lane 01 (LOW, unsettled).
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

- 📋 [DFR-0055] **Copy the fixed git hooks in from the project template.**
  .githooks/commit-msg, pre-commit, pre-push and scripts/setup-hooks.sh are
  byte-identical to ~/.claude/skeleton/files (diff -q, 2026-11-16).
  local-ci.sh:27-29 and :58-67 and ci.yml:24 carry template text. Their
  defects went to the claude-config session (owner's choice, 2026-11-16):
  the pre-commit link checker drifted from check-links.py and checks the
  working tree, a predictable /tmp name, advice to use --no-verify, word
  splitting on spaces, a dated-line exemption matching any line, three
  copies of one pipeline, and a wrong comment on how the push hook reads
  DOCS_GLOB. Do this once claude-config reports the template fixed.
  **Layman:** Once the shared template's hook defects are fixed, copy the fixed files here so the two stay identical.
  Kind: chore.
  Source: review-code-2026-11-16.
  Lanes: Build.

- 📋 [DFR-0056] **Settle the protocol spec's open questions in one amendment.**
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
  **Layman:** Decide six small rules the engine-window protocol leaves open, and correct two stale lines in the design.
  Kind: doc.
  Source: review-code-2026-11-16.
  Lanes: Records.

- 📋 [DFR-0057] **Write DeFragR's trust boundaries into SECURITY.md.**
  SECURITY.md's trust-boundary list is still empty. The review recommends
  recording, as accepted by design: a root report sends the user's window
  the paths inside folders that user cannot read; the short mount-id reuse
  window between probe_find_mount and walk_start (reaching it already needs
  admin authentication); and that names from a drive are untrusted input
  (see the plain-text item). Confirm each with the owner while writing it.
  Found by review-code lanes 01 and 02.
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

- 📋 [DFR-0059] **Run the gate's tests in parallel, if it measures faster.**
  local-ci.sh:183 runs ctest with no -j, so the three test-machine tests
  run one after another. Serial total 63.95 s, longest test 30.85 s
  (2026-11-16), so -j saves at most about 33 s, less once the 30 s wait
  above is fixed, and less if image tests contend on the spinning drive.
  Measure ctest -j against serial, alternating, same nice/ionice. DFR-0037
  declined -j before the VM tests existed. Do after the tst_engineprocess
  item. Found by review-code --performance lane 03.
  **Layman:** Try running the pre-push tests side by side, and keep it only if it is really quicker.
  Kind: perf.
  Source: review-code-perf-2026-11-16.
  Lanes: Build.

- 📋 [DFR-0060] **Measure a cold report's disk pattern before reordering the walk.**
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
  **Layman:** Measure where a report on a big spinning drive spends its time, before trying to make it faster.
  Kind: investigate.
  Source: review-code-perf-2026-11-16.
  Lanes: Walk, Entry.

- 📋 [DFR-0061] **Report view: cheaper live updates and column sizing.**
  - ReportView.cpp:105 updates the "latest file" line once per fragmented
    file (up to about 30,600 on /mnt/Games): elide, two labels, a tooltip,
    a relayout. Paint it from the existing 1 s tick.
  - ReportView.cpp:46 ResizeToContents sizes columns from rows, calling
    Report::data per cell. Size once after load, or bound the precision.
    Qt's resizeContentsPrecision default is unverified.
  - Report.cpp:190 decodes a path each time a File cell is drawn.
  Measure on a replayed 30k-row report first. Found by review-code
  --performance lane 02.
  **Layman:** Keep the window smooth while a report on a big drive streams in thousands of results.
  Kind: perf.
  Source: review-code-perf-2026-11-16.
  Lanes: Report view, Report.

- 📋 [DFR-0062] **Gate and container: smaller build costs.**
  - local-ci.sh:164 builds a fresh tree every push; median 4.7 s against
    DFR-0037's 1.7 s warm-ccache figure; NASM, moc and links are never
    cached. Measure with ccache -z/-s and .ninja_log first.
  - ci-container.sh:119 copies the whole kernel module tree; run.sh needs
    the index and four modules.
  - ci.yml:62: man-db's trigger could be turned off (from the lane's
    memory of Ubuntu; check first).
  Found by review-code --performance lane 03.
  **Layman:** Small savings in how long the safety checks take to build.
  Kind: perf.
  Source: review-code-perf-2026-11-16.
  Lanes: Build.

- 📋 [DFR-0063] **A test that clicks the window's real buttons.**
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
  **Layman:** An automatic check that presses the window's own buttons, so a break between the screen and the engine is caught before release.
  Kind: test.
  Source: verify-delivery-2026-11-16.
  Lanes: Screens, Report view.

- 📋 [DFR-0064] **Fill in the README's summary, Install and Usage.**
  README.md line 3 is still "> One line: what this does, for whom.",
  § Install is "(Once there is something to install.)" and § Usage is
  "(Once there is something to use.)". 0.1.0 can be built and used from
  source, so say what it does and how to build and run it.
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
