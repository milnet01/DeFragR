# Changelog

All notable changes to DeFragR are documented in this file.

The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The format
contract is `~/.claude/standards/changelog-format.md` § 4.

The `[Unreleased]` block stays at the top, always, even when empty.

## [Unreleased]

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
