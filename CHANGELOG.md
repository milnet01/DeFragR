# Changelog

All notable changes to DeFragR are documented in this file.

The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The format
contract is `~/.claude/standards/changelog-format.md` § 4.

The `[Unreleased]` block stays at the top, always, even when empty.

## [Unreleased]

### Security
- A report refuses a folder that is not a real drive, such as a system
  area, a memory-only folder or a network share, instead of reading it
  with admin rights (DFR-0044).
- Drive and file names always show as written. A name that looks like
  web code is no longer drawn as a picture or a link (DFR-0047).
- A report never opens a device, and never wakes a network or
  on-demand drive mounted inside the one being read. Before, an
  on-demand drive there could make the report hang (DFR-0045).
- Another user on the same computer can no longer stall a report, stop
  the drives being listed, or make a report read only part of a drive
  (DFR-0073).
- A file or drive name can no longer change how the window shows text:
  not through a tooltip, and not with hidden characters that reverse the
  reading direction, which now show as codes such as [U+202E]. A report's
  memory is capped, so a drive crowded with fragmented files cannot use
  up the computer's memory; past the cap, the list keeps the worst files
  and says how many it left out (DFR-0074).

### Added
- A "Refresh drives" button lists the drives again, and the window says
  "Listing drives…" while it does (DFR-0049).
- The window speaks the desktop's language: Hebrew, German, Italian,
  Greek, Spanish, Portuguese (Brazil and Portugal), Chinese (Simplified
  and Traditional), Japanese, Russian, Polish, French or Ukrainian, with
  English otherwise. These are AI drafts not yet checked by native
  speakers; corrections are welcome (DFR-0067).
- Headings are a step larger than the rest of the text, and text is
  never smaller than 10 points, even on a desktop set smaller
  (DFR-0013).
- A report shows a map of the drive as a grid of squares: whole files
  solid, fragmented files striped, folders and the filesystem dotted,
  free space as an outline. A key to the patterns sits right above the
  map. Each square shows what fills most of it. The map rolls in as a
  bright band from the first block to the last, and a square that
  changes lights up and glows over its neighbours before settling
  (DFR-0014, DFR-0071).

### Fixed
- A report on a drive that was moved onto another drive's folder is no
  longer refused, and a strange answer from the system about a file's
  pieces or the free space can no longer hang a report (DFR-0078).
- When the engine is missing, broken, or there but unable to start, the
  window says so once, at once, and in the right words. An engine whose
  output breaks the rules no longer leaves a report "running" until it
  has read the whole drive (DFR-0077).
- The drive map never flashes more than three times a second, however
  often a block changes. Resizing the window while the map fills in
  shows the whole map at once, with no new glow (DFR-0075).
- When pkexec, the program that asks for admin rights, is missing, the
  window says so, instead of asking you to enter a password no prompt
  will ask for. Advice to check the drive list again names the new
  "Refresh drives" button (DFR-0049).
- In a right-to-left language such as Hebrew, folder paths read as
  written, instead of with their first slash moved to the end
  (DFR-0069).
- An engine that answers a different question than it was asked is
  reported as a fault, not shown as an empty report. An error the system
  has no words for reads "no details given", not "Success" or a number
  (DFR-0051).
- A drive whose folder name holds characters that are not valid text
  can be reported on, instead of being called "not the top of a drive"
  (DFR-0048).
- The drive list stays whole when one drive cannot be read: that drive
  is left out, instead of the list coming back empty. A drive mounted
  over another is listed once, as the one you see there, and a network
  or add-on drive can no longer hide a real drive from the list
  (DFR-0046).
- Snap packages' disk images are no longer listed as drives. A drive's
  size is right on a filesystem that counts space in a unit other than
  its block size. An engine from a different release is reported as
  one, even when its first message is cut short (DFR-0056).

## [0.1.0] - 2026-11-16

**Theme:** Read-only report

### Added
- The window lists your drives and offers "View report" or "Defrag
  now" for each. An action a drive cannot take is greyed out, with the
  reason in plain words. "Defrag now" says defragging comes in a later
  version (DFR-0005).
- The engine measures how scattered each file on a drive is, without
  changing anything. It stays on the one drive and does not follow
  shortcuts (symlinks) to other places (DFR-0008).
- "View report" shows the report in the same window: the share of
  files that are fragmented, the average number of pieces, how many
  files could not be read, and every fragmented file, worst first
  (DFR-0009).
- A running report shows how far it has got: files and folders checked,
  of about how many, with a percentage bar (DFR-0043).
- The window shows DeFragR's own icon (DFR-0039).

### Fixed
- When the system blocks the engine after you enter your password, the
  window no longer says admin rights were refused (DFR-0042).
