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
- **A hidden command checks the engine's checksum (1.3, DFR-0016).**
  Claude's choice. It tests the shipped engine itself, as
  `selftest-records` does.
- **`defrag` moves the files in one folder and every folder below it
  (1.4, DFR-0017).** Claude's choices: the folder includes its
  sub-folders on the same drive, so a whole drive is the folder `""`; each
  move of up to `MOVE_CHUNK_BLOCKS` blocks (`src/engine/move.asm`) sends
  where its blocks came from and went, so the map can show each journey;
  a file written by someone else during its move is skipped, not a
  checksum failure. The owner's choice (2026-11-18): a CPU without
  SSE4.2 cannot checksum, so its drives are report only.

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
defines **1.7**: 1.0 plus `REPORT_PROGRESS` in 1.1, `MAP_RUNS` in 1.2
(§ 4.6), `selftest-crc32c` in 1.3 (§ 4.11), `defrag` in 1.4 (§ 4.12),
`defrag`'s map and `MAP_DONE` in 1.5, `DEFRAG_PROGRESS` in 1.6
(§ 4.12), and `MAP_RUNS` kind 5 in 1.7 (§ 4.6).

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
defragr-engine defrag <mount-point> <folder> [map]
                                      defrag a folder and below (§ 4.12); needs root
defragr-engine selftest-records       fixed records (§ 4.9); hidden
defragr-engine selftest-crc32c        checksum test (§ 4.11); hidden
```

Arguments are the only text the engine parses. Every run, including a
bad one, writes a record stream to standard output and nothing else.
Standard error is for a crash only.

`report`'s mount point is written as `/proc/self/mountinfo` writes one:
a backslash and three octal digits stand for one byte. The engine
decodes it before any check (INV-15). Qt starts the engine with text
arguments, so this is how a name that is not UTF-8 reaches it.
`defrag`'s mount point is written the same way, and so is its folder:
a path relative to the mount point, its names joined by single `/`, with
no leading or trailing `/` and no name `.` or `..`. The empty folder is
the mount point's own. A fourth word `map` asks for the drive's map
first (§ 4.12). Any other fourth word, or a fifth, is `USAGE`.

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
2       2     minor        5
4       4     command      0 unknown, 1 probe, 2 report, 3 defrag, 254 selftest-crc32c, 255 selftest-records
8       4     block_size   the drive's statfs f_bsize in bytes; non-zero in a report or defrag whose result is OK; 0 for probe, or when not known
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
0       4     errno     signed; the failing call's errno, positive; 0 for stages 10 and 11
4       2     stage     1 open, 2 stat, 3 extents, 4 read folder, 5 path too long;
                        in defrag also 6 to 11 (§ 4.12)
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
0           4     kind     1 free, 2 the filesystem's own, 3 a file in one piece, 4 a file in two or more
                           pieces, 5 such a file whose pieces are large
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
- Kinds 3, 4 and 5 come from each visited file's extents
  (`FS_IOC_FIEMAP`). Kinds 4 and 5 are a file in two or more pieces, as
  `FRAGMENTED_FILE` counts them. It is kind 5 where its blocks, as
  `FRAGMENTED_FILE` counts `size_blocks`, times the block size are at
  least its pieces times `LARGE_PIECE_BYTES`, 16 MiB: its average piece
  is that large. Otherwise it is kind 4. Added in 1.7; an engine before
  it sends kind 4 for both. 16 MiB is a judgement, measured on no drive
  (ROADMAP DFR-0106). An extent flagged unknown or not aligned
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
| 9 | `INTERNAL` | The engine broke one of its own limits, could not ignore `SIGPIPE` (§ 4.3), got a wrong checksum in `selftest-crc32c` (§ 4.11), or a write failed with an error other than `EPIPE` (exit status only, § 6) |
| 10 | `NO_FOLDER` | `defrag`'s folder is not written as § 4.3 says (`errno` 22, `EINVAL`), or cannot be opened on that drive without following a link or crossing a mount; `errno` says why |
| 11 | `CHECKSUM_MISMATCH` | A file read back after its move differs from before; the run stopped. The `FILE_SKIPPED` just before `FINISHED` names it |

`INTERNAL`'s causes are each a bug to report, so they share one code.

The engine checks in this order, and reports the first that fails:
arguments, admin rights, mount point, drive rules, then `defrag`'s
folder.

**Reason codes** (`DRIVE.reason`), in the order the engine picks one:

| Code | Name | Meaning |
|---|---|---|
| 0 | `NONE` | Defrag allowed |
| 1 | `FS_NOT_EXT4` | Not ext4; `fs_type` names it |
| 2 | `READ_ONLY` | Mounted read-only |
| 3 | `SSD` | The drive is solid-state |
| 4 | `MEDIA_UNKNOWN` | The engine cannot tell whether it is solid-state |
| 5 | `OLD_CPU` | The processor lacks SSE4.2, whose `crc32` instruction the checksum needs (§ 4.12) |

`defrag` on a drive whose reason is not 0 finishes `REFUSED`.

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
or a resume while running, sends nothing. The safe point is between
files: in a defrag, once a file's turn has ended and its records are
sent (§ 4.12, *Stopping*). DFR-0117 builds `CANCEL` and the closed
standard input for a defrag. DFR-0023 builds the rest of the engine's
half; until then a defrag skips `PAUSE` and `RESUME`, and no other run
reads a command.

### 4.9 `selftest-records`

Emits, in this order, one record of every engine type with these values,
then exits 0. Each multi-byte value has distinct bytes, so a swapped,
misplaced or mis-sized field shows. The exceptions are fixed by meaning:
`HELLO`'s major, minor and command, `MAP_RUNS`' count, and `FINISHED`'s
result and errno, and `BLOCKS_MOVED`' counts.

| Record | Field values |
|---|---|
| `HELLO` | major 1, minor 7, command 255, block_size `0x01020304` |
| `DRIVE` | total_blocks `0x1112131415161718`, free_blocks `0x2122232425262728`, block_size `0x31323334`, dev_major `0x41424344`, dev_minor `0x51525354`, media 2, verdict 1, reason `0xD1D2`, mount_flags `0xE1E2E3E5`, mount_point the 7 bytes `/mnt/t` then `0xFF`, source empty, fs_type `ext4` |
| `FRAGMENTED_FILE` | extents `0x61626364`, size_blocks `0x7172737475767778`, path `a/b` |
| `FILE_SKIPPED` | errno `0x0A0B0C0D`, stage `0x0E0F`, path `c` |
| `REPORT_SUMMARY` | files `0x8182838485868788`, fragmented_files `0x9192939495969798`, extents `0xA1A2A3A4A5A6A7A8`, file_blocks `0xB1B2B3B4B5B6B7B8`, skipped `0xC1C2C3C4C5C6C7C8` |
| `REPORT_PROGRESS` | names `0xE1E2E3E4E5E6E7E8`, estimate `0xF1F2F3F4F5F6F7F8` |
| `MAP_RUNS` | kind `0x13141516`, count 2, run 0 start `0x3132333435363738` and length `0x4142434445464748`, run 1 start `0x5152535455565758` and length `0x6162636465666768` |
| `RUN_STATE` | state `0xF1F2F3F4` |
| `FILE_DEFRAGGED` | extents_before `0x21222324`, extents_after `0x25262728`, moved_blocks `0x2A2B2C2D2E2F3031`, path `d/e` |
| `BLOCKS_MOVED` | from_count 1, to_count 2, from run start `0x7172737475767778` and length `0x8182838485868788`, to run 0 start `0x9192939495969798` and length `0xA1A2A3A4A5A6A7A8`, to run 1 start `0xB1B2B3B4B5B6B7B8` and length `0xC1C2C3C4C5C6C7C8` |
| `MAP_DONE` | none: its fixed part is reserved |
| `DEFRAG_PROGRESS` | files_done `0xD1D2D3D4D5D6D7D8`, files_total `0xD9DADBDCDDDEDFE0`, blocks_done `0xC9CACBCCCDCECFD0`, blocks_total `0xB9BABBBCBDBEBFC0` |
| `FINISHED` | result 0, errno 0 |

All reserved fields, flags and padding are 0. `BLOCKS_MOVED`' lengths
break § 4.12's equal-blocks rule on purpose: the decoder does not check
it. The `HELLO` record is these 24 bytes:

```
01 00 00 00 10 00 00 00   type 1, flags 0, length 16
01 00 07 00 ff 00 00 00   major 1, minor 7, command 255
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
§ 4.5's for 1, § 4.6's and INV-8 for 2, § 4.12's and INV-19 for 3, and
only § 4.3's for any other value. An `Unknown` record may sit anywhere between `HELLO` and
`FINISHED`, and no order or count rule counts it.

### 4.11 `selftest-crc32c`

Sends `HELLO` with command 254 and block_size 0, then `FINISHED`. The
result is `OK` when the engine's CRC32C gives each value below, and
`INTERNAL` when one differs. CRC32C starts from `0xFFFFFFFF` and inverts
its result.

| Bytes | CRC32C | Source |
|---|---|---|
| none | `0x00000000` | the starting value |
| `123456789` | `0xE3069283` | the CRC catalogue's check value |
| 32 of `0x00` | `0x8A9136AA` | RFC 3720 appendix B.4 |
| 32 of `0xFF` | `0x62A8AB43` | RFC 3720 appendix B.4 |
| `0x00` to `0x1F`, rising | `0x46DD794E` | RFC 3720 appendix B.4 |
| `0x1F` to `0x00`, falling | `0x113FDB5C` | RFC 3720 appendix B.4 |
| RFC 3720's 48-byte read command | `0xD9963A56` | RFC 3720 appendix B.4 |

`123456789` is also checked in two pieces, the second continuing from
the first, and from an address that is not a multiple of 8.

### 4.12 Defrag records

The defrag stream is `HELLO`, then `FILE_DEFRAGGED`, `FILE_SKIPPED`,
`BLOCKS_MOVED`, `DEFRAG_PROGRESS` and `RUN_STATE` in any order, then
`FINISHED`. Added in 1.4; `DEFRAG_PROGRESS` in 1.6.

**With `map`** (§ 4.3), the drive's map comes straight after `HELLO`:
`REPORT_PROGRESS` and `MAP_RUNS` as a report sends them (§ 4.6), then
one `MAP_DONE`. Added in 1.5, so a window asks for the drive's map and
its defrag in one run, with admin rights asked for once.

- The engine checks the folder first, so `NO_FOLDER` comes before
  `HELLO` and before the walk.
- It then walks the whole drive to read, as `report` does. A file or
  folder it cannot read sends no record: its blocks are in no run.
- It then opens the mount point and the folder again, to move. A drive
  or folder that changed meanwhile ends the run after `MAP_DONE`, with
  the result § 4.7 gives it.
- No `MAP_RUNS` or `REPORT_PROGRESS` follows `MAP_DONE`, or a file's
  record.
- A run that ends before `MAP_DONE` has sent part of a map. A reader
  shows none of it.

**`MAP_DONE`** — type `0x0025`, fixed part 8 bytes, all reserved, 0.

The engine visits each regular file in the folder and the folders below
it, on that drive only, as `report` walks (`src/engine/walk.asm`). A file
in one piece gets no record. A file in two or more:

1. is checked: still a regular file, on the drive's own mount, not
   immutable or append-only. The kernel refuses a swap file at the move;
2. is synced, so data not yet written has its blocks;
3. is read whole from the drive, bypassing the cache, for its CRC32C;
4. gets a donor: a file with no name (`O_TMPFILE`) in the same folder,
   given the file's size in one `fallocate`. A donor in no fewer pieces
   than the file ends the file's turn: it is left as it was;
5. swaps blocks with the donor (`EXT4_IOC_MOVE_EXT`), one move of up to
   `MOVE_CHUNK_BLOCKS` blocks at a time;
6. is synced and read back from the drive. It must have the same size,
   modification time and CRC32C as at step 3.

A crash leaves the file readable as it was: the kernel swaps blocks and
writes their data in one journal transaction per page
(`mext_move_extent`, the kernel's `fs/ext4/move_extent.c`, Linux 7.3),
and frees a donor with no name when the drive is next mounted.
Unverified by any test here.

**Stopping.** A defrag stops between files, never inside a file's
turn, so every file it began was read back and checked (DFR-0117).

- Before each file's turn the engine reads the commands that have
  arrived, without waiting for one. On `CANCEL`, or a standard input at
  its end, it begins no further file and sends `FINISHED` with
  `CANCELLED`.
- A `CANCEL` that arrives during the map or the count takes effect
  before the first file's turn.
- A write that fails with `EPIPE` during a file's turn does not end the
  turn. The engine sends nothing more, takes the file through step 6,
  and exits with `CANCELLED`; or with `CHECKSUM_MISMATCH` where step
  6's CRC32C differs.
- A write that fails outside a file's turn ends the run at that write
  (§ 6).
- No record or command is added, so the version does not change.

**`DEFRAG_PROGRESS`** — type `0x0042`, fixed part 32 bytes. How far the
defrag has got, so a window can show a bar. Added in 1.6.

```
offset  size  field
0       8     files_done    files in two or more pieces whose turn has ended
8       8     files_total   such files in the folder when it was counted
16      8     blocks_done   the blocks those finished files held
24      8     blocks_total  the blocks all such files held when counted
```

- Before any file's record, and after the map when one is sent, the
  engine walks the folder once to count, changing nothing, and sends
  the first `DEFRAG_PROGRESS`: both done fields 0. It then opens the
  mount point and the folder again to move, with every check of the
  first time; a drive or folder that changed ends the run with the
  result § 4.7 gives it.
- It sends one more after the `FILE_DEFRAGGED` or `FILE_SKIPPED` of each
  file it found in two or more pieces: one file and that file's blocks
  further on, the totals unchanged.
- A file's blocks are those its extents cover, as `FRAGMENTED_FILE`
  counts `size_blocks`, read before its move.
- A file the walk cannot open is in neither count. Its `FILE_SKIPPED`
  is followed by no `DEFRAG_PROGRESS`.
- The folder can change between the count and the move, so done may
  end below total or pass it. A reader shows no more than the whole.
- A defrag from an engine before 1.6 sends none.

**`FILE_DEFRAGGED`** — type `0x0040`, fixed part 16 bytes, then one
string. Sent for each file in two or more pieces that the engine
finished with and did not skip.

```
offset  size  field
0       4     extents_before  pieces before, counted as FRAGMENTED_FILE counts them
4       4     extents_after   pieces after
8       8     moved_blocks    blocks whose place changed; 0 when no free space would
                              hold the file in fewer pieces, and it was left as it was
16      str   path            relative to the mount point
```

**`BLOCKS_MOVED`** — type `0x0041`, fixed part 8 bytes, then
`from_count` runs, then `to_count` runs, each laid out as a `MAP_RUNS`
run (§ 4.6). Sent after each move that changed where any block of the
file lies.

```
offset                   size  field
0                        4     from_count  runs the blocks left
4                        4     to_count    runs they went to
8 + 16 × i               16    from run i
8 + 16 × (from_count + j) 16   to run j
```

Both lists hold the same number of blocks, in the file's order: block k
of the from runs went to block k of the to runs. The from runs are free
once the file's turn ends. A file's `BLOCKS_MOVED` all come straight
before its `FILE_DEFRAGGED` or `FILE_SKIPPED`. A count whose runs pass
the payload's end is malformed.

**`FILE_SKIPPED`** in a defrag adds these stages:

| Stage | Name | When |
|---|---|---|
| 6 | refused | Step 1 failed: `errno` is `EPERM` for immutable or append-only, `EXDEV` for another mount, `EINVAL` for not a regular file |
| 7 | read | A checksum read failed |
| 8 | donor | The donor could not be made or measured; `ENOSPC` when the drive lacks the room |
| 9 | move | A sync or a move failed. Blocks moved before it are in `BLOCKS_MOVED` records, and the file still reads as it did |
| 10 | changed | The file's size or modification time changed during its turn: someone wrote to it. `errno` 0 |
| 11 | checksum differs | Step 6's CRC32C differs, with size and time unchanged. `errno` 0; `FINISHED` follows with `CHECKSUM_MISMATCH` |

A drive whose `DRIVE.reason` is `OLD_CPU` gets no defrag: the `crc32`
instruction would stop the engine.

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
  error and no trailing bytes, and exits with `FINISHED.result`. A run
  whose write fails is the exception (§ 6): it stops at that write,
  prints nothing, and exits with `CANCELLED` on `EPIPE` and `INTERNAL`
  on any other error. A defrag's write that fails with `EPIPE` inside a
  file's turn is INV-29's.
  *Test:* `tests/app/tst_records_conformance.cpp`, run without root —
  runs `defragr-engine` and `defragr-engine bogus` (each expects result
  1), `report` with no mount point (expects 1, so arguments come before
  admin rights), `report /nonexistent` (expects 2, so admin rights come
  before the mount point), `selftest-records`, and `selftest-crc32c x`
  (expects 1), and checks each exit
  status against `result`. `failedWriteIsInternal` runs `bogus` and
  `selftest-records` with standard output on `/dev/full` and expects
  exit 9. `closedPipeIsCancelled` gives `selftest-records` a pipe with
  no reader and expects exit 7. Both expect nothing on standard error.
  *Breaks when:* the engine prints text, writes a record after
  `FINISHED`, exits with a different code, or dies of `SIGPIPE`.

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
  `filefrag -v` lists for files in one piece, and kinds 4 and 5 those of
  files in two or more, each file's as INV-28 says. Block 0, the superblock's, is in a kind 2 run.
  *Test:* `tests/image/report-check.cpp`, the `image_report` test. The
  image's free space lies in more runs than one GETFSMAP call returns
  (`FSMAP_PER_CALL`, `src/engine/extents.asm`), so a second call is
  needed.
  *Breaks when:* the engine reads a byte offset as a block, sends
  GETFSMAP's unowned blocks as the filesystem's, marks a fragmented
  file's runs as whole, or stops after the first GETFSMAP call.

- **INV-18** — `selftest-crc32c` sends `HELLO` with command 254, then
  `FINISHED` with `OK`, and exits 0.
  *Test:* `tests/app/tst_records_conformance.cpp::selftestCrc32c`, which
  runs the built engine.
  *Breaks when:* `src/engine/crc32c.asm` gives any § 4.11 value wrong,
  including when bytes come in pieces or start unaligned.

- **INV-19** — A defrag stream (`HELLO.command` 3) accepts
  `FILE_DEFRAGGED`, `FILE_SKIPPED`, `BLOCKS_MOVED` and `RUN_STATE`
  between `HELLO` and `FINISHED`, and with result `OK` needs a non-zero
  `HELLO.block_size`. Any other known record there is malformed, as is a
  `BLOCKS_MOVED` whose counts pass its payload's end.
  *Test:* `tests/app/tst_records.cpp::defragStream` — a valid stream
  holding one of each; the same with a `REPORT_SUMMARY` added; then its
  `BLOCKS_MOVED` with `to_count` one higher in the same payload, whose
  length stays valid, so only the count rule can reject it.
  *Breaks when:* the decoder reads `0x0040` or `0x0041` as `Unknown`,
  trusts the counts, or lets a report's records into a defrag.

- **INV-20** — On ext4, a `defrag` that finishes `OK` leaves every file's
  SHA-256 as it was and `e2fsck -fn` clean. Each file with a
  `FILE_DEFRAGGED` whose `moved_blocks` is not 0 is in fewer pieces by
  `filefrag`, and `extents_after` is `filefrag`'s count. Its
  `BLOCKS_MOVED` from runs cover exactly the blocks `filefrag -v` listed
  before, and its to runs those it lists after, each in the file's
  order. No file outside the folder moves, and no new name appears.
  *Test:* `tests/image/defrag-check.cpp`, the `image_defrag` test, on
  `build_image`'s files plus one big enough for two moves. Run once on
  the folder `deep`, then on the whole drive.
  *Breaks when:* the engine damages data, moves a file outside the
  folder, leaves a named donor, reports runs in bytes or in the wrong
  order, or reports a move of blocks that did not move.

- **INV-21** — `defrag` checks in § 4.7's order. Without root it
  finishes `USAGE` for a wrong argument count and `NOT_ROOT` otherwise.
  As root: a sub-folder of a drive that would refuse is `NOT_A_MOUNT`;
  that drive is `REFUSED`, even with a folder that does not exist; on a
  drive that allows defrag, a folder that does not exist, one written
  with `..`, and one that is a symbolic link are each `NO_FOLDER`.
  *Test:* `tests/app/tst_records_conformance.cpp` for the first two;
  `tests/image/check-defrag.sh`, the `image_defrag` test, for the rest,
  on an SSD test disk and a spinning one.
  *Breaks when:* the drive rules run before the mount-point check, the
  folder before the drive rules, or a folder is followed through a link
  or out of the drive.

- **INV-22** — On a CPU without SSE4.2, `probe` gives an ext4 drive that
  would otherwise allow defrag verdict 1 and reason `OLD_CPU`; `defrag`
  on it finishes `REFUSED`, and `report` finishes `OK`.
  *Test:* `tests/image/check-old-cpu.sh`, the `image_defrag` test, in a
  test machine whose CPU has SSE4.2 taken away
  (`run.sh --cpu-without sse4.2`).
  *Breaks when:* the engine runs `crc32` anyway and dies, or refuses
  `report` too.

- **INV-23** — `defrag` moves no file it must not. A file in one piece
  gets no record. An immutable file and a swap file in use each get a
  `FILE_SKIPPED`, and keep their extents and SHA-256.
  *Test:* `tests/image/check-defrag.sh`, the `image_defrag` test, with
  both made fragmented on purpose.
  *Breaks when:* the engine moves a file its checks should stop, or
  the kernel's swap-file refusal goes unreported.

- **INV-24** — In a defrag stream, `MAP_RUNS`, `REPORT_PROGRESS` and
  one `MAP_DONE` are accepted before the first `FILE_DEFRAGGED`,
  `FILE_SKIPPED` or `BLOCKS_MOVED`. A `MAP_RUNS`, `REPORT_PROGRESS` or
  `MAP_DONE` after `MAP_DONE` or after a file's record is malformed, as
  is a `MAP_DONE` with no fixed part, and a `MAP_DONE` in a report.
  *Test:* `tests/app/tst_records.cpp::defragStreamWithMap` — a valid
  stream, then the same with one record moved or added, so only the
  order rule can reject it.
  *Breaks when:* the decoder reads `0x0025` as `Unknown`, or lets the
  map resume after the files begin.

- **INV-25** — `defrag <mount-point> <folder> map` sends
  `REPORT_PROGRESS` with no names straight after `HELLO`, then
  `MAP_RUNS` naming the blocks of each kind that a `report` run just
  before it named, then one `MAP_DONE`, then defrags as INV-20 says.
  Without `map` it sends none of the three. A fourth word other than
  `map`, or a fifth, is `USAGE`. With `map`, a folder that does not
  exist is `NO_FOLDER` with no record but `HELLO` and `FINISHED`.
  *Test:* `tests/image/defrag-check.cpp` and
  `tests/image/check-defrag.sh`, the `image_defrag` test: the defrag of
  `deep` asks for the map, and the whole-drive one does not.
  `tests/app/tst_records_conformance.cpp` for the argument rule without
  root.
  *Breaks when:* the engine skips GETFSMAP's runs, walks only the
  folder for the map, sends `MAP_DONE` before the map's last runs, or
  reads the map before it has checked the folder.

- **INV-26** — In a defrag stream a `DEFRAG_PROGRESS` is accepted
  wherever a file's record is, and closes the map as one does: a
  `MAP_RUNS`, `REPORT_PROGRESS` or `MAP_DONE` after it is malformed. A
  `DEFRAG_PROGRESS` whose fixed part is short is malformed, as is one in
  a report. Done past total is accepted.
  *Test:* `tests/app/tst_records.cpp::defragProgressRecords`.
  *Breaks when:* the decoder reads `0x0042` as `Unknown`, or lets the
  map resume after it.

- **INV-27** — A `defrag` sends its first `DEFRAG_PROGRESS` before any
  `BLOCKS_MOVED`, `FILE_DEFRAGGED` or `FILE_SKIPPED`, with both done
  fields 0. Its files_total is the folder's files that `filefrag` shows
  in two or more pieces, less any the walk cannot open, and its
  blocks_total the blocks `filefrag` lists for them. Each later one
  follows a file's record and is one file and more blocks further on,
  with the totals unchanged. The last has done equal to total.
  *Test:* `tests/image/defrag-check.cpp`, the `image_defrag` test, on
  each of its defrags. `tests/app/tst_records_conformance.cpp` for the
  record's bytes.
  *Breaks when:* the engine counts files in one piece, counts after it
  has moved a file, or sends progress for a file that was not work.

- **INV-28** — A report sends a file in two or more pieces as kind 5
  where its blocks times the block size are at least its pieces times
  16 MiB, by `filefrag`'s pieces and blocks, and as kind 4 otherwise.
  *Test:* `tests/image/report-check.cpp`, the `image_report` test. The
  image holds `large`, its space reserved in two goes with another
  file's between, which the test machine must show in pieces averaging
  16 MiB or more; `build_image`'s fragmented files are in one-block
  pieces.
  *Breaks when:* the engine compares blocks with bytes, never sends
  kind 5, or sends it for every file in pieces. The fixture does not
  tell an average from a largest piece, or from the file's size.

- **INV-29** — A defrag begins no file's turn after `CANCEL` or the end
  of its standard input, and finishes with `CANCELLED`. A file whose
  turn had begun when a write failed with `EPIPE` is taken through
  step 6, and the engine then exits with `CANCELLED`.
  *Test:* the `image_defrag` test (`tests/image/check-defrag.sh`,
  `tests/image/defrag-check.cpp`), in three runs DFR-0117 writes. They
  are not yet written.
  One run's standard input is at its end from the start. One is sent
  `CANCEL` first, its input then held open. Each must send no
  `BLOCKS_MOVED`, finish with `CANCELLED`, and leave every block of
  every file where it was. The rule each isolates: the engine reads its
  input before a file's turn; an engine that reads none moves the
  files.
  One run defrags a folder holding one file of many moves, its input
  held open and its output closed once the first `BLOCKS_MOVED` is
  read. It must exit with `CANCELLED`; the file must read the same and
  none of its blocks may lie where it did. The rule it isolates: the
  turn goes on after the failed write; an engine that exits there
  leaves the later moves' blocks in place. The file must be long enough
  that the engine is still moving it when the output closes, which the
  test cannot see; a shorter file passes either engine.
  *Breaks when:* the engine never reads its input, takes an input at
  its end for no command, or exits at the failed write with the file
  part moved.

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
  `CANCELLED`. Its reads see end of input, which is `CANCEL` too. In a
  defrag, a file whose turn has begun is finished and checked first
  (§ 4.12, *Stopping*).
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
- **A file is written during its move.** Its time or size changes, so
  it is skipped at stage 10, not taken for damage (§ 4.12).
- **A file reads back different after its move.** The run stops with
  `CHECKSUM_MISMATCH`, naming the file, so no more files are touched.
- **The drive fills during a defrag.** The donor's `fallocate` fails
  with `ENOSPC`; that file is skipped at stage 8 and the run goes on.

## 7. Tests

DFR-0003 writes the first two and sees each fail first, against a
decoder missing the rule it locks. DFR-0008 writes the third and sees it
fail against an engine that accepts any folder under a mount point.

| Test | Locks | Needs root |
|---|---|---|
| `tests/app/tst_records.cpp` | INV-1 to INV-9, INV-13, INV-16, INV-19, INV-24, INV-26 | No |
| `tests/app/tst_records_conformance.cpp` | INV-10, INV-11, INV-18, INV-21 and INV-25's arguments without root | No |
| `tests/image/check-report.sh` | INV-12, INV-15 | Yes, inside the test machine |
| `tests/app/tst_engineprocess.cpp` | INV-15, the window's half | No |
| `tests/image/report-check.cpp` | INV-14, INV-17, INV-28 | Yes, its input comes from the test machine |
| `tests/image/check-defrag.sh` | INV-21 as root, INV-23, INV-25's arguments and folder check, INV-29's runs | Yes, inside the test machine |
| `tests/image/defrag-check.cpp` | INV-20, INV-22's probe record, INV-25's map, INV-27, INV-29's streams | Yes, its input comes from the test machine |
| `tests/image/check-old-cpu.sh` | INV-22 | Yes, inside a test machine with no SSE4.2 |

DFR-0043 writes INV-13's and INV-14's tests and sees each fail first.
DFR-0011 does the same for INV-16's and INV-17's, DFR-0016 for INV-18's,
DFR-0017 for INV-19's to INV-23's, and DFR-0096 for INV-24's and
INV-25's. DFR-0099 writes INV-26's and INV-27's. INV-27's was seen
failing on 2026-11-19 against an engine that counted files in one piece
too. INV-26's was not seen failing. DFR-0106 writes INV-28's, seen
failing on 2026-11-19 against an engine that sent `large` as kind 4.
DFR-0117 writes INV-29's.

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

- Whole-drive defrag's progress, and naming a file before its move —
  tracked by DFR-0020.
- A defrag stream holds no map: the window takes it from a report —
  tracked by DFR-0018.
- The engine reading commands — tracked by DFR-0023.
- The window's session log format — deferred; not yet queued.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 to INV-9, INV-13, INV-16, INV-19, INV-24, INV-26 | `tests/app/tst_records.cpp` |
| INV-10, INV-11, INV-18 | `tests/app/tst_records_conformance.cpp` |
| INV-12 | `tests/image/check-report.sh` (`image_report`) |
| INV-14, INV-17, INV-28 | `tests/image/report-check.cpp` (`image_report`) |
| INV-15 | `tests/image/check-report.sh` (`image_report`); `tests/app/tst_engineprocess.cpp::pathArgument` |
| INV-20 | `tests/image/defrag-check.cpp` (`image_defrag`) |
| INV-21 | `tests/app/tst_records_conformance.cpp`; `tests/image/check-defrag.sh` (`image_defrag`) |
| INV-22 | `tests/image/check-old-cpu.sh` and `tests/image/defrag-check.cpp` (`image_defrag`) |
| INV-23 | `tests/image/check-defrag.sh` (`image_defrag`) |
| INV-25 | `tests/image/defrag-check.cpp` and `tests/image/check-defrag.sh` (`image_defrag`); `tests/app/tst_records_conformance.cpp` |
| INV-27 | `tests/image/defrag-check.cpp` (`image_defrag`); `tests/app/tst_records_conformance.cpp` for the bytes |
| INV-28 | `tests/image/report-check.cpp` (`image_report`) |
| INV-29 | `tests/image/check-defrag.sh` and `tests/image/defrag-check.cpp` (`image_defrag`), once DFR-0117 writes its runs |
| § 4.12 `DEFRAG_PROGRESS` when the folder changes between the count and the move | **nothing**: it needs a change timed between the two walks |
| § 4.12 with `map`, a drive or folder that changes during the map's walk | **nothing**: it needs a change timed between the two walks |
| § 4.7 check order | INV-11 and INV-21 |
| § 4.12 stages 7 to 11, `CHECKSUM_MISMATCH`, a donor in no fewer pieces | **nothing**: each needs a failing drive, a writer racing the move, or a full drive |
| § 4.7 reason order | `tests/app/tst_probe.cpp::reasonFollowsTheRules`, on the drives of the machine it runs on |
| § 4.8 `PAUSE` and `RESUME` | **nothing** until DFR-0023 |
| § 4.8 `CANCEL` in a defrag | INV-29 |
| § 6 failed write | INV-11's `failedWriteIsInternal` and `closedPipeIsCancelled` |
| § 4.3 failed `SIGPIPE` setup | **nothing**: `rt_sigaction` fails only on a bad argument (`sigaction(2)`), and the engine's are fixed |

## 11. Cross-doc impact

- `ROADMAP.md`: DFR-0002's body says "every record and command"; it
  changes to name § 3's split. DFR-0011, DFR-0017 and DFR-0020 each gain
  a line: amend this spec with their records. DFR-0008 gains INV-12's
  image test, and DFR-0017 the § 4.7 check-order test that `REFUSED`
  makes reachable.
- `tests/CMakeLists.txt`: `engine_runs` runs the bare engine and passes
  only on exit 0. Under § 4.3 that run exits 1 (`USAGE`), so DFR-0003
  replaces it with INV-11's conformance test.
- `docs/design.md` § Safety: 1.4 settles the `O_TMPFILE` donor that it
  marks unverified.
- `docs/design.md` § Units: unchanged. `HELLO.block_size` is how "its
  first record states the block size" holds; it is 0 for `probe`, whose
  `DRIVE` records state their own.

## 12. Cold-eyes loop log

Rows live in `../reviews/DFR-0002-engine-protocol-loop-log.md`.
