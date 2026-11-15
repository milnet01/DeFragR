# DeFragR — Discovery

> **Purpose — so that later, anyone can tell whether the thing being
> built is still the thing that was wanted.**

**Status:** agreed (2026-11-15).

## The problem

Linux already has a defrag tool for ext4 drives (`e4defrag`). It works,
but it runs in the terminal and prints plain lines of text. Watching it
is boring, and it does not show the drive at all.

It also does not say which drives are worth defragging. Defragging an
SSD wears it and gains nothing. Defragging btrfs can bloat its snapshots.

## Who it is for

- **The owner first:** a person with two large spinning ext4 drives full
  of games and ROMs (`/mnt/Games`, `/mnt/Emulators`). They want to pick a
  drive, see how fragmented it is, and watch it being fixed in a window
  that is pleasant, even hypnotic, to look at. They are partially
  sighted, so the display must be readable without relying on colour.
- **When finished (1.0):** any Linux desktop user who downloads it from
  the public GitHub repository, and wants a clear, safe, good-looking
  defrag for spinning drives.

## Signs it is working

- **S1** — Opening the app lists every mounted drive, with its size,
  filesystem, and whether it is an SSD or a spinning drive.
- **S2** — At the start you choose either "view report" or "defrag now".
- **S3** — "View report" shows how fragmented the drive is and its
  worst-affected files, and changes nothing on the drive.
- **S4** — A window shows a map of the drive as blocks. During a defrag
  the blocks visibly change from fragmented to whole, and it is
  something you would happily sit and watch.
- **S5** — A status bar names the file being worked on right now.
- **S6** — A percentage and a progress bar climb to 100% as the defrag
  runs.
- **S7** — After defragging `/mnt/Games`, a new report shows fewer
  fragmented files, and every file still opens and matches its
  checksum from before.
- **S8** — The PC stays usable while a defrag runs, and pause and
  cancel take effect without damaging anything.
- **S9** — Picking an SSD, a btrfs drive or an unsupported filesystem
  gives a report only, with a plain reason why it will not defrag.
- **S10** — The map and text are readable by a partially sighted user:
  high contrast, symbols as well as colours, and large enough.
- **S11** — Another Linux user can install it from a GitHub release and
  run it without help.
- **S12** — Every button and option responds to what is selected. Only
  actions that apply are available. For example, selecting an SSD leaves
  "View report" usable, and "Defrag" disabled or hidden.

## What it deliberately does not do

**Never** (each confirmed by the owner, 2026-11-15):

- **Defrag an SSD.** It reports on one only.
- **Move file data itself.** It uses only the Linux kernel's own checked
  move. No home-made block shuffling, and no unmounted drives.
- **Defrag Windows drives** (NTFS, FAT).
- **Pack all files to the front of the drive** to merge free space.
  Linux has no safe way to do it.

**Not yet:**

- **Defrag btrfs on a spinning drive.** Report only until then. It must
  first detect snapshots and shared copies, which a defrag would
  un-share and so use more space.
- **A terminal view** beside the window.
- **A command-line mode** for scripts, and scheduled runs.
- **XFS support.**
