<!-- ants-spec-format: 1 -->
# DFR-0002 — Define the engine–window record protocol

**Status:** accepted (2026-11-15).
**Kind:** doc.
**Source:** ROADMAP DFR-0002 (design-2026-11-15; ADR-0001 names this spec
as the one home of the record layout).
**Blocker for:** DFR-0003.
**Layman:** No user-visible change; this fixes how the engine and the
window talk, so both halves can be built to it.

## 1. Goal

One document says, byte by byte, what the engine writes, what the window
accepts, and what each code means. `src/engine/records.inc` and
`src/app/engine/Records.h` are both written from it, and the
`selftest-records` conformance test proves they agree.

## 2. Problem

ADR-0001 decides that the engine and the window talk in binary records
over pipes, and that the layout's one home is a spec in `docs/specs/`.
There is no such spec. Without it:

1. The engine (assembly) and the window (C++) would each invent a layout,
   and the two would drift (ADR-0001, *Consequences*).
2. ADR-0002 makes the engine's verdict and reason code the only source of
   which actions a drive allows (S9, S12). With no fixed codes, the window
   cannot map them to plain words (`model/Messages`, `docs/design.md`).
3. `docs/design.md` § Errors says a malformed record is a broken engine
   and an unknown record type is skippable. Neither is decidable until
   "malformed" and "type" are defined.

## 3. Scope decisions (agreed with the user)

- **This spec fixes the framing, versioning, codes, the 0.1.0 records and
  the pause, resume and cancel commands. Later items add their records to
  it by amendment** — the drive map (DFR-0011), moving files (DFR-0017)
  and progress (DFR-0020). The user chose this on 2026-11-15, over fixing
  every record now: those records depend on kernel behaviour no one has
  tested yet (`docs/design.md` § Safety marks the `O_TMPFILE` donor
  unverified).
- **The engine sends every fragmented file; the window ranks the worst.**
  Claude's choice. It keeps sorting out of the assembly.
- **Paths are sent relative to the mount point.** Claude's choice. They
  are shorter, and the window already knows the mount point.
- **One reason per drive, chosen by a fixed order** (§ 4.7). Claude's
  choice. The window shows one sentence, so the engine must pick which.
- **A report sends how far it has got (1.1, DFR-0043).** The user asked
  for it on 2026-11-16. Claude's choices: the engine counts names, not
  files, and estimates the total from the drive's used inodes, which
  `statfs` gives without a walk.
- **A report sends the drive map as runs of blocks (1.2, DFR-0011).**
  Claude's choices: the window bins runs into cells (`docs/design.md`
  § The parts, *Map*), so the engine sends positions, not cells; many runs
  share a record, since a large drive has millions; a run says whether
  its file is fragmented, so the map needs no file names.

## 4. Design

### 4.1 Framing

Every record, in both directions, is an 8-byte header and a payload. All
integers are little-endian and unsigned unless marked signed.

```
offset  size  field
0       2     type     record type (§ 4.4, § 4.8). 0 is invalid.
2       2     flags    0. Writers send 0; readers ignore it.
4       4     length   payload bytes. A multiple of 8, at most 65536.
8       len   payload
```

Each record therefore starts at a multiple of 8 bytes from the start of
the stream.

**A payload** is a fixed part, then zero or more strings or, in
`MAP_RUNS`, its runs, then optional trailing bytes. The fixed part's
fields sit at the offsets each record's table gives.

**A string** is a `u32` byte count, then that many raw bytes, then zero
bytes up to the next multiple of 8 from the payload's start. Bytes are
not NUL-terminated and need not be UTF-8; a path is the kernel's bytes.
A string carries at most 4096 bytes.

**Trailing bytes** after the last field a reader knows are ignored. This
is how a minor version appends a field (§ 4.2).

A reader treats as **malformed**:

- type 0;
- a `length` not a multiple of 8, or above 65536;
- a payload shorter than its type's fixed part;
- a string whose count exceeds 4096 or runs past the payload's end.

Padding bytes and `flags` are not checked.

### 4.2 Versioning

The `HELLO` record (§ 4.4) carries a major and a minor version. This spec
defines **1.2**: 1.0 plus `REPORT_PROGRESS` in 1.1 and `MAP_RUNS` in 1.2
(§ 4.6).

- **Minor** changes add, and never change: a new record or command type,
  a field appended to a payload, a new value of an enumerated field. An
  amendment adding one bumps the minor.
- **Major** changes anything an existing reader relies on. A window
  supports major 1 only. Any other major ends the run as a **version
  mismatch**, which is not a malformed stream: the window says the
  engine and window come from different releases. The major is read
  before `HELLO`'s length is checked, since another major may lay the
  rest out differently.

A reader handles a value it does not know in an enumerated field as
"unknown", never as malformed (INV-9).

### 4.3 Running the engine

```
defragr-engine probe                  list drives; no root needed
defragr-engine report <mount-point>   report on one drive; needs root
defragr-engine selftest-records       fixed records (§ 4.9); hidden
```

Arguments are the only text the engine parses. Every run, including a
bad one, writes a record stream to standard output and nothing else.
Standard error is for a crash only.

`report`'s mount point is written as `/proc/self/mountinfo` writes one:
a backslash and three octal digits stand for one byte. The engine
decodes it before any check (INV-15). Qt starts the engine with text
arguments, so this is how a name that is not UTF-8 reaches it.

The stream is:

```
HELLO
  (records for the command, in the order § 4.5 and § 4.6 give)
FINISHED
```

`HELLO` comes first and `FINISHED` last, once each. The engine then exits
with `FINISHED.result` as its exit status. A write that fails ends the
run with no `FINISHED` (§ 6). The engine ignores `SIGPIPE` before it
reads the command. If that fails, it sends `HELLO` with command 0, then
`FINISHED` with `INTERNAL` and the call's errno. An unknown command still gets
a stream: `HELLO` with command 0, then `FINISHED` with `USAGE`. A known
command with the wrong number of arguments sends `HELLO` with its own
command, then `FINISHED` with `USAGE`.

### 4.4 Records every run sends

**`HELLO`** — type `0x0001`, fixed part 16 bytes.

```
offset  size  field
0       2     major        1
2       2     minor        2
4       4     command      0 unknown, 1 probe, 2 report, 255 selftest-records
8       4     block_size   the drive's statfs f_bsize in bytes; non-zero in a report whose result is OK; 0 for probe, or when not known
12      4     reserved     0
```

All positions and lengths in later records are in blocks of
`block_size` (`docs/design.md` § Units). A `DRIVE` record states its own,
`f_frsize`. A FUSE filesystem answers each separately (`fuse_kstatfs` in
`<linux/fuse.h>`), so a reader never mixes a report's unit with a probe's.

**`FINISHED`** — type `0x0002`, fixed part 8 bytes.

```
offset  size  field
0       4     result   signed; a result code (§ 4.7)
4       4     errno    signed, positive; the failing call's errno, else 0
```

### 4.5 Probe records

**`DRIVE`** — type `0x0010`, fixed part 40 bytes, then three strings.

```
offset  size  field
0       8     total_blocks   statfs f_blocks
8       8     free_blocks    statfs f_bfree
16      4     block_size     statfs f_frsize, the unit of f_blocks and f_bfree
20      4     dev_major      the mounted device
24      4     dev_minor
28      1     media          0 unknown, 1 SSD, 2 spinning
29      1     verdict        1 report only, 2 report and defrag
30      2     reason         § 4.7; 0 when verdict is 2, non-zero when it is 1
32      4     mount_flags    bit 0: mounted read-only
36      4     reserved       0
40      str   mount_point    absolute
..      str   source         the mount source, e.g. /dev/sda1
..      str   fs_type        e.g. ext4, btrfs
```

The probe stream is `HELLO`, zero or more `DRIVE`, `FINISHED`. No two
`DRIVE` records share a `mount_point`. The window enables defrag only
on verdict 2 (ADR-0002); DFR-0005 tests that. Which mounts count as drives, and
which rule yields which reason, are set inside this layout by
`src/engine/probe.asm`'s header (DFR-0004).

### 4.6 Report records

The report stream is `HELLO`, then `FRAGMENTED_FILE`, `FILE_SKIPPED`,
`REPORT_PROGRESS`, `MAP_RUNS` and `RUN_STATE` in any order, then — only
when the result is `OK` — one `REPORT_SUMMARY`, then `FINISHED`.

**`FRAGMENTED_FILE`** — type `0x0020`, fixed part 16 bytes, then one
string. Sent for each regular file with two or more extents.

```
offset  size  field
0       4     extents       physically separate runs, counted as filefrag counts them
4       4     reserved      0
8       8     size_blocks   blocks its extents cover, each rounded up to whole blocks; an inline-data file counts 1
16      str   path          relative to the mount point, no leading /
```

**`FILE_SKIPPED`** — type `0x0021`, fixed part 8 bytes, then one string.
Sent for each file or folder the engine could not read. The run goes on.

```
offset  size  field
0       4     errno     signed, positive
4       2     stage     1 open, 2 stat, 3 extents, 4 read folder, 5 path too long
6       2     reserved  0
8       str   path      relative to the mount point; its first 4096 bytes
```

A path longer than 4096 bytes is skipped with stage 5 and errno 36
(`ENAMETOOLONG`).

**`REPORT_SUMMARY`** — type `0x0022`, fixed part 40 bytes.

```
offset  size  field
0       8     files              regular files whose extents FIEMAP gave
8       8     fragmented_files   equals the FRAGMENTED_FILE records sent
16      8     extents            the extents of every file counted in files, whole or fragmented, summed
24      8     file_blocks        the same files' blocks, each counted as size_blocks is, summed
32      8     skipped            equals the FILE_SKIPPED records sent
```

**`REPORT_PROGRESS`** — type `0x0023`, fixed part 16 bytes. How far the
walk has got. Added in 1.1.

```
offset  size  field
0       8     names      names the walk has read in the drive's folders, of any type, not counting . and ..
8       8     estimate   the drive's used inodes when the walk began, statfs f_files − f_ffree; 0 when f_files is 0 or below f_ffree
```

The engine sends one straight after `HELLO`, with `names` 0. When the
result is `OK`, one more comes straight before `REPORT_SUMMARY`, counting
every name. Between those it sends others between files; how often is
the engine's choice (`src/engine/main.asm`), and a reader relies on no
rate.

`names` can end above or below `estimate`. A hard link adds a name but no
inode; inodes the filesystem keeps for itself have no name. On
2026-11-16, btrfs and vfat mounts gave `f_files` 0 (`stat -f -c %c`).

**`MAP_RUNS`** — type `0x0024`, fixed part 8 bytes, then `count` runs of
16 bytes. Where the drive's blocks lie, for the map. Added in 1.2.

```
offset      size  field
0           4     kind     1 free, 2 the filesystem's own, 3 a file in one piece, 4 a file in two or more pieces
4           4     count    runs after the fixed part
8 + 16 × i  8     start    run i's first block
16 + 16 × i 8     length   run i's blocks
```

A `count` whose runs pass the payload's end is malformed. Bytes after
the last run are trailing bytes (§ 4.1).

- Kinds 1 and 2 come from `FS_IOC_GETFSMAP`, read once before the walk.
  Kind 2 is what the filesystem keeps for itself: superblocks, group
  tables, bitmaps, inode tables, the journal. Blocks in use that
  GETFSMAP gives no owner, as ext4 gives file data, are not sent.
- Blocks ext4 has set aside for files being written are in no run:
  GETFSMAP counts them in use, `statfs` counts them free. Mounting afresh
  lets them go.
- A drive that cannot give that map sends no runs of kinds 1 and 2, or
  only those read before GETFSMAP failed. The report goes on.
- Kinds 3 and 4 come from each visited file's extents
  (`FS_IOC_FIEMAP`). Kind 4 is a file in two or more pieces, as
  `FRAGMENTED_FILE` counts them. An extent flagged unknown or not aligned
  has no blocks of its own yet, and is not sent. A file skipped at stage
  3 may have sent some runs first.
- A run starting where the last run of its kind ends is joined onto it,
  so one run can span several files.
- A block in no run is in use by something unnamed: a folder, an extent
  tree, a skipped file, or a part of the drive GETFSMAP did not describe.
- Runs can overlap. A file is visited once per hard link, and the drive
  can change while the report runs.

**`RUN_STATE`** — type `0x0030`, fixed part 8 bytes. Sent when the engine
acts on a pause or resume command (§ 4.8).

```
offset  size  field
0       4     state      1 running, 2 paused
4       4     reserved   0
```

### 4.7 Codes

**Result codes** (`FINISHED.result`, and the exit status):

| Code | Name | Meaning |
|---|---|---|
| 0 | `OK` | The command finished |
| 1 | `USAGE` | Missing or unknown arguments |
| 2 | `NOT_ROOT` | The command needs admin rights and has none |
| 3 | `NOT_A_MOUNT` | The path given is not the mount point of a drive |
| 4 | `MOUNTINFO` | `/proc/self/mountinfo` could not be read |
| 5 | `DRIVE_ERROR` | A drive-level call failed; `errno` says which error |
| 6 | `REFUSED` | The drive's verdict does not allow this command |
| 7 | `CANCELLED` | Cancelled by command or by a closed standard input |
| 8 | `BAD_COMMAND` | A malformed command arrived on standard input |
| 9 | `INTERNAL` | The engine broke one of its own limits, could not ignore `SIGPIPE` (§ 4.3), or a write failed with an error other than `EPIPE` (exit status only, § 6) |

Each of those causes is a bug to report, so they share one code.

The engine checks in this order, and reports the first that fails:
arguments, admin rights, mount point, drive rules.

**Reason codes** (`DRIVE.reason`), in the order the engine picks one:

| Code | Name | Meaning |
|---|---|---|
| 0 | `NONE` | Defrag allowed |
| 1 | `FS_NOT_EXT4` | Not ext4; `fs_type` names it |
| 2 | `READ_ONLY` | Mounted read-only |
| 3 | `SSD` | The drive is solid-state |
| 4 | `MEDIA_UNKNOWN` | The engine cannot tell whether it is solid-state |

### 4.8 Commands

Commands go from the window to the engine's standard input, framed as
§ 4.1. Each has an empty payload.

| Type | Name | Effect |
|---|---|---|
| `0x0101` | `PAUSE` | Stop work at the next safe point; send `RUN_STATE` paused |
| `0x0102` | `RESUME` | Carry on; send `RUN_STATE` running |
| `0x0103` | `CANCEL` | Stop at the next safe point; finish with `CANCELLED` |

A closed standard input is `CANCEL`. An unknown command type is skipped.
A malformed command ends the run with `BAD_COMMAND`. A pause while paused,
or a resume while running, sends nothing. In a report, the safe point is
between files. DFR-0023 builds the engine's half.

### 4.9 `selftest-records`

Emits, in this order, one record of every engine type with these values,
then exits 0. Each multi-byte value has distinct bytes, so a swapped,
misplaced or mis-sized field shows. The exceptions are fixed by meaning:
`HELLO`'s major, minor and command, `MAP_RUNS`' count, and `FINISHED`'s
result and errno.

| Record | Field values |
|---|---|
| `HELLO` | major 1, minor 2, command 255, block_size `0x01020304` |
| `DRIVE` | total_blocks `0x1112131415161718`, free_blocks `0x2122232425262728`, block_size `0x31323334`, dev_major `0x41424344`, dev_minor `0x51525354`, media 2, verdict 1, reason `0xD1D2`, mount_flags `0xE1E2E3E5`, mount_point the 7 bytes `/mnt/t` then `0xFF`, source empty, fs_type `ext4` |
| `FRAGMENTED_FILE` | extents `0x61626364`, size_blocks `0x7172737475767778`, path `a/b` |
| `FILE_SKIPPED` | errno `0x0A0B0C0D`, stage `0x0E0F`, path `c` |
| `REPORT_SUMMARY` | files `0x8182838485868788`, fragmented_files `0x9192939495969798`, extents `0xA1A2A3A4A5A6A7A8`, file_blocks `0xB1B2B3B4B5B6B7B8`, skipped `0xC1C2C3C4C5C6C7C8` |
| `REPORT_PROGRESS` | names `0xE1E2E3E4E5E6E7E8`, estimate `0xF1F2F3F4F5F6F7F8` |
| `MAP_RUNS` | kind `0x13141516`, count 2, run 0 start `0x3132333435363738` and length `0x4142434445464748`, run 1 start `0x5152535455565758` and length `0x6162636465666768` |
| `RUN_STATE` | state `0xF1F2F3F4` |
| `FINISHED` | result 0, errno 0 |

All reserved fields, flags and padding are 0. The `HELLO` record is
these 24 bytes:

```
01 00 00 00 10 00 00 00   type 1, flags 0, length 16
01 00 02 00 ff 00 00 00   major 1, minor 2, command 255
04 03 02 01 00 00 00 00   block_size 0x01020304, reserved 0
```

### 4.10 Where each half lives

- **Engine:** `src/engine/records.inc` holds the type, code and offset
  constants; `src/engine/records.asm` writes records and reads commands.
- **Window:** `src/app/engine/Records.h` declares one struct per record
  and a decoder fed bytes in any chunks:

```cpp
namespace defragr::engine {
enum class StreamError { Malformed, VersionMismatch };
class RecordDecoder {
public:
    // Appends bytes; decodes every complete record they finish.
    void feed(QByteArrayView bytes);
    // Decoded records not yet taken, in stream order.
    QList<Record> takeRecords();
    // Input has ended; runs the end-of-stream checks.
    void finish();
    // Set once the stream is malformed or a version mismatch; sticky.
    std::optional<StreamError> error() const;
};
}
```

`Record` is a `std::variant` of the record structs plus an `Unknown`
type that keeps the type number. The decoder checks stream-order rules
too, at `FINISHED` or at `finish()`. `HELLO.command` picks them:
§ 4.5's for 1, § 4.6's and INV-8 for 2, and only § 4.3's for any other
value. An `Unknown` record may sit anywhere between `HELLO` and
`FINISHED`, and no order or count rule counts it.

## 5. Invariants

Each clause names the test that locks it and the rule its fixture
isolates.

- **INV-1** — The decoder reports malformed for type 0, for a `length`
  not a multiple of 8, and for a `length` above 65536; it accepts an
  unknown type with length 0 and with length 65536.
  *Test:* `tests/app/tst_records.cpp::framing` — hand-built headers, each
  differing from a valid one in one field only.
  *Breaks when:* a decoder reads `length` 12 as 12 bytes and carries on.

- **INV-2** — A record of an unknown type with a valid frame decodes as
  `Unknown` carrying its type number, and the record after it decodes
  correctly.
  *Test:* `tests/app/tst_records.cpp::unknownTypeSkipped` — a valid
  report stream (`HELLO.command` 2) with a type `0x00FF` record of
  length 16 between `HELLO` and `FINISHED`.
  *Breaks when:* the decoder ends the stream on a type it does not know,
  or skips the wrong number of bytes.

- **INV-3** — A known record whose payload is shorter than its fixed part
  is malformed; one longer than its fields is decoded and the extra bytes
  are ignored.
  *Test:* `tests/app/tst_records.cpp::payloadLength` — `FINISHED` with
  length 0, and with length 16 and non-zero trailing bytes.
  *Breaks when:* the decoder reads past a short payload, or rejects a
  longer one from a newer minor.

- **INV-4** — A string's bytes come through unchanged, including `0xFF`
  and an empty string; a string count above 4096, or running past the
  payload, is malformed.
  *Test:* `tests/app/tst_records.cpp::strings` — `DRIVE` payloads whose
  only fault is the string count. The fixed part is valid, so only the
  string rule can reject them.
  *Breaks when:* the decoder converts the bytes as UTF-8, or trusts the
  count.

- **INV-5** — Decoding a stream gives the same records and the same error
  however its bytes are split into `feed()` calls.
  *Test:* `tests/app/tst_records.cpp::chunking` — the self-test stream
  fed whole, one byte at a time, and split at every offset in turn.
  *Breaks when:* the decoder assumes a header or payload arrives in one
  read.

- **INV-6** — The stream is malformed when a record precedes `HELLO`,
  when `HELLO` or `FINISHED` repeats, when a record follows `FINISHED`,
  or when input ends with no `FINISHED`.
  *Test:* `tests/app/tst_records.cpp::streamOrder` — one case per rule,
  each otherwise a valid stream.
  *Breaks when:* the window treats a crashed engine's cut-off stream as
  a finished run.

- **INV-7** — A `HELLO` with major other than 1 is a version mismatch,
  not malformed, even when too short for 1.x; minor 7 with major 1 is
  accepted.
  *Test:* `tests/app/tst_records.cpp::version`.
  *Breaks when:* a newer minor is refused, or a new major is decoded as
  if it were 1.

- **INV-8** — A report stream (`HELLO.command` 2) with result `OK` has a
  non-zero `HELLO.block_size` and holds exactly one `REPORT_SUMMARY`,
  with no known record but `FINISHED` after it, whose `fragmented_files`
  and `skipped` equal the counts of `FRAGMENTED_FILE` and `FILE_SKIPPED`
  records; otherwise the stream is malformed. An `Unknown` record may
  follow it (§ 4.10), so a later minor can add one there.
  *Test:* `tests/app/tst_records.cpp::summaryCounts` — a stream with two
  `FRAGMENTED_FILE` records and a summary claiming three, otherwise valid.
  *Breaks when:* the engine loses count, and the window shows a summary
  that disagrees with its own list.

- **INV-9** — Unknown values of `command`, `media`, `verdict`, `reason`,
  `stage`, `result` and `state` decode without error. A `DRIVE` with
  verdict 2 and a non-zero reason, or verdict 1 and reason 0, is
  malformed; an unknown verdict may carry any reason.
  *Test:* `tests/app/tst_records.cpp::enumerations` — `DRIVE` records
  with reason 200, verdict 3 with reason 0, and the two mismatched pairs.
  *Breaks when:* the decoder rejects a verdict or reason added by a
  later minor, or accepts a mismatched pair.

- **INV-10** — The `selftest-records` stream decodes, with no error, to
  exactly the records and values in § 4.9, and the engine exits 0.
  *Test:* `tests/app/tst_records_conformance.cpp` — runs the built engine
  and compares every field. The values are distinct per field, so only a
  layout disagreement between the two halves fails it.
  *Breaks when:* `records.inc` and `Records.h` disagree on any type,
  offset, size or padding, outside the fields § 4.9 names as fixed by
  meaning.

- **INV-11** — Every engine run writes a stream that decodes with no
  error and no trailing bytes, and exits with `FINISHED.result`.
  *Test:* `tests/app/tst_records_conformance.cpp`, run without root —
  runs `defragr-engine` and `defragr-engine bogus` (each expects result
  1), `report` with no mount point (expects 1, so arguments come before
  admin rights), `report /nonexistent` (expects 2, so admin rights come
  before the mount point) and `selftest-records`, and checks each exit
  status against `result`.
  *Breaks when:* the engine prints text, writes a record after
  `FINISHED`, or exits with a different code.

- **INV-12** — Trust boundary: the engine reports on a path only after
  finding it as a mount point in `/proc/self/mountinfo` whose mount is a
  drive by probe's test (§ 4.5); a path that is not one finishes
  `NOT_A_MOUNT` with no record naming a file.
  *Test:* `tests/image/check-report.sh`, the `image_report` test, run as
  root in the test machine: `report /mnt/img/deep` on a mounted image,
  where only the mount-point rule can fail (the path exists and the
  caller is root); and `report` on a tmpfs mount point, whose stream must
  equal the sub-folder's byte for byte.
  *Breaks when:* the engine accepts any folder and walks it, so a
  crafted path reaches files on another drive; or it accepts any mount
  point, so it walks `/proc`, a network share or a FUSE mount with no
  block device as root.

- **INV-13** — A `REPORT_PROGRESS` in a report stream decodes with its
  fields and counts toward neither of INV-8's counts. One after
  `REPORT_SUMMARY` is malformed.
  *Test:* `tests/app/tst_records.cpp::reportProgress` — a valid report
  stream with two `REPORT_PROGRESS` records and a summary claiming no
  files, so only a decoder that counts them as files or skips fails it.
  *Breaks when:* the decoder reads type `0x0023` as `Unknown`, swaps its
  fields, or counts it into the summary check.

- **INV-14** — A report that finishes `OK` sends `REPORT_PROGRESS` first
  after `HELLO`, with `names` 0 and `estimate` the drive's used inodes,
  and last straight before `REPORT_SUMMARY`, with `names` equal to every
  name below the mount point on its own mount. A mount point inside it
  counts as one name, and nothing below that counts.
  *Test:* `tests/image/report-check.cpp`, the `image_report` test. The
  image holds a symbolic link, which the walk reads but never visits.
  The used inodes come from `stat -f` before the run; the names from
  `find -mindepth 1 -xdev` after it.
  *Breaks when:* the engine counts only what it visits, counts `.` and
  `..`, or sends `f_ffree` as the estimate.

- **INV-15** — `report` decodes its argument's `\` and three octal
  digits to one byte before matching a mount point. The window escapes
  every backslash, and every byte above `0x7F` when the path is not
  UTF-8.
  *Test:* `tests/image/check-report.sh`, the `image_report` test: a bind
  mount of the image at a name holding `0xFF`, a space and a backslash,
  given escaped, must exit `OK` with the first report's stream byte for
  byte. `tests/app/tst_engineprocess.cpp::pathArgument` checks the
  window's escaping.
  *Breaks when:* the engine matches the raw argument, or the window
  passes the path through `QFile::decodeName`, which turns a byte that
  is not UTF-8 into U+FFFD.

- **INV-16** — A `MAP_RUNS` decodes with its kind and every run, whatever
  the kind's value, and counts toward neither of INV-8's counts. One whose
  `count` passes its payload's end is malformed, as is one after
  `REPORT_SUMMARY`.
  *Test:* `tests/app/tst_records.cpp::mapRuns` — a report stream holding
  a kind 1 record of two runs and a kind 200 record of none, with a
  summary claiming no files. Then the first record with `count` 3 in the
  same 40-byte payload: its length is valid, so only the count rule can
  reject it.
  *Breaks when:* the decoder trusts `count` and reads past the payload,
  reads type `0x0024` as `Unknown`, or rejects a kind added later.

- **INV-17** — On ext4 mounted afresh since its last write, with no hard
  links, a report that finishes `OK` sends runs that share no block.
  Kind 1 covers as many blocks as `statfs` gives free. Kind 3 covers
  exactly the blocks
  `filefrag -v` lists for files in one piece, and kind 4 those of files
  in two or more. Block 0, the superblock's, is in a kind 2 run.
  *Test:* `tests/image/report-check.cpp`, the `image_report` test. The
  image's free space lies in more runs than one GETFSMAP call returns
  (`FSMAP_PER_CALL`, `src/engine/extents.asm`), so a second call is
  needed.
  *Breaks when:* the engine reads a byte offset as a block, sends
  GETFSMAP's unowned blocks as the filesystem's, marks a fragmented
  file's runs as whole, or stops after the first GETFSMAP call.

## 6. Failure modes

- **The engine crashes.** No `FINISHED` arrives; INV-6 makes it a broken
  engine, and the window says so.
- **`pkexec` is refused.** `pkexec` exits 126 or 127 before the engine
  starts, so there is no `HELLO`. The window reads an empty stream with
  that exit status as "admin rights refused", not as a broken engine.
  That mapping is DFR-0005's.
- **`pkexec` cannot be started.** No password prompt appeared, so the
  window says `pkexec` did not start, not that admin rights were
  refused (DFR-0049).
- **The window dies.** The engine's writes fail with `EPIPE`, because
  `sys.asm` ignores `SIGPIPE`; the engine exits at once with
  `CANCELLED`. Its reads see end of input, which is `CANCEL` too.
- **A write is partial or interrupted.** The engine repeats `write`
  until the whole record is out, retrying on `EINTR`.
- **A write fails with another error, or makes no progress.** No
  `FINISHED` can follow, so the engine exits with `INTERNAL` (§ 4.7). The
  window reads the cut-off stream as a broken engine (INV-6).
- **The stream breaks a rule mid-run.** The window ends the run when the
  record that breaks it arrives, not when the engine exits. It ends as a
  broken engine, or as a version mismatch when `HELLO` names another
  major (INV-7). The window kills an engine running as the user. One
  running as root, which it cannot kill, ends at its next write once its
  pipes close (*The window dies*).
  `tests/app/tst_engineprocess.cpp::brokenStreamEndsAtOnce` and
  `::brokenStreamViaLauncherEndsAtOnce` check the window's half.
- **Engine and window from different releases.** A new major is a
  version mismatch (INV-7); a new minor reads as unknown types and
  values (INV-2, INV-9).
- **A file name is not UTF-8.** It travels as bytes (INV-4); the window
  decodes it with `QFile::decodeName` for display.
- **A mount point is not UTF-8.** The window sends it to `report`
  escaped (§ 4.3, INV-15).

## 7. Tests

DFR-0003 writes the first two and sees each fail first, against a
decoder missing the rule it locks. DFR-0008 writes the third and sees it
fail against an engine that accepts any folder under a mount point.

| Test | Locks | Needs root |
|---|---|---|
| `tests/app/tst_records.cpp` | INV-1 to INV-9, INV-13, INV-16 | No |
| `tests/app/tst_records_conformance.cpp` | INV-10, INV-11 | No |
| `tests/image/check-report.sh` | INV-12, INV-15 | Yes, inside the test machine |
| `tests/app/tst_engineprocess.cpp` | INV-15, the window's half | No |
| `tests/image/report-check.cpp` | INV-14, INV-17 | Yes, its input comes from the test machine |

DFR-0043 writes INV-13's and INV-14's tests and sees each fail first.
DFR-0011 does the same for INV-16's and INV-17's.

`tst_records.cpp` exercises the decoder only; it proves nothing about the
engine. The conformance and image tests read real engine output.

## 8. Alternatives considered (and rejected)

- **Text lines** — ADR-0001 rejected them: assembly parses text badly.
- **A type-length-value list per field** — self-describing, but the
  assembly would write a tag per field, and fixed offsets already let
  old readers skip new fields (§ 4.2).
- **No padding to 8 bytes** — smaller, but every `u64` read in the
  assembly would need care for alignment. Padding costs at most 7 bytes
  per string.
- **The engine ranks the worst files** — a sort in assembly, for a list
  the window can rank in one line.
- **A separate version handshake from the window** — one direction is
  enough: the window can refuse the engine's major, and the engine reads
  only fixed commands.

## 9. Out of scope

- Defrag's arguments and records: file moved, blocks changed, checksum
  failure — tracked by DFR-0017.
- Defrag's progress records — tracked by DFR-0020.
- The engine reading commands — tracked by DFR-0023.
- The window's session log format — deferred; not yet queued.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 to INV-9, INV-13, INV-16 | `tests/app/tst_records.cpp` |
| INV-10, INV-11 | `tests/app/tst_records_conformance.cpp` |
| INV-12 | `tests/image/check-report.sh` (`image_report`) |
| INV-14, INV-17 | `tests/image/report-check.cpp` (`image_report`) |
| INV-15 | `tests/image/check-report.sh` (`image_report`); `tests/app/tst_engineprocess.cpp::pathArgument` |
| § 4.7 check order | INV-11 up to the mount point; mount point before drive rules has **nothing**: both verdicts allow `report`, so `REFUSED` cannot happen until defrag exists — tracked by DFR-0017 |
| § 4.7 reason order | `tests/app/tst_probe.cpp::reasonFollowsTheRules`, on the drives of the machine it runs on |
| § 4.8 commands | **nothing** until DFR-0023 |
| § 4.3 failed `SIGPIPE` setup; § 6 failed write | **nothing** |

## 11. Cross-doc impact

- `ROADMAP.md`: DFR-0002's body says "every record and command"; it
  changes to name § 3's split. DFR-0011, DFR-0017 and DFR-0020 each gain
  a line: amend this spec with their records. DFR-0008 gains INV-12's
  image test, and DFR-0017 the § 4.7 check-order test that `REFUSED`
  makes reachable.
- `tests/CMakeLists.txt`: `engine_runs` runs the bare engine and passes
  only on exit 0. Under § 4.3 that run exits 1 (`USAGE`), so DFR-0003
  replaces it with INV-11's conformance test.
- `docs/design.md` § Units: unchanged. `HELLO.block_size` is how "its
  first record states the block size" holds; it is 0 for `probe`, whose
  `DRIVE` records state their own.

## 12. Cold-eyes loop log

Rows live in `../reviews/DFR-0002-engine-protocol-loop-log.md`.
