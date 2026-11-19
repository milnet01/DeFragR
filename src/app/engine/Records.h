// Records: the window's half of the engine-window protocol. Decodes the
// engine's record stream, fed in any chunks, into one struct per record.
// The one home of the layout is docs/specs/DFR-0002-engine-protocol.md;
// the engine's half is src/engine/records.inc. selftest-records and
// tests/app/tst_records_conformance.cpp prove the two agree.
#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QList>

#include <optional>
#include <variant>

namespace defragr::engine {

// § 4.1 framing limits.
inline constexpr int kHeaderSize = 8;
inline constexpr quint32 kPayloadMax = 65536;
inline constexpr quint32 kStringMax = 4096;

// The protocol major this window reads (§ 4.2).
inline constexpr quint16 kMajor = 1;

// Record types (§ 4.4 to § 4.6, § 4.12).
enum RecordType : quint16 {
    TypeHello = 0x0001,
    TypeFinished = 0x0002,
    TypeDrive = 0x0010,
    TypeFragmentedFile = 0x0020,
    TypeFileSkipped = 0x0021,
    TypeReportSummary = 0x0022,
    TypeReportProgress = 0x0023,
    TypeMapRuns = 0x0024,
    TypeRunState = 0x0030,
    TypeFileDefragged = 0x0040,
    TypeBlocksMoved = 0x0041,
};

// HELLO.command (§ 4.4).
enum Command : quint32 {
    CommandUnknown = 0,
    CommandProbe = 1,
    CommandReport = 2,
    CommandDefrag = 3,
    CommandSelftestCrc32c = 254,
    CommandSelftest = 255,
};

// FINISHED.result (§ 4.7).
enum Result : qint32 {
    ResultOk = 0,
    ResultUsage = 1,
    ResultNotRoot = 2,
    ResultNotAMount = 3,
    ResultMountinfo = 4,
    ResultDriveError = 5,
    ResultRefused = 6,
    ResultCancelled = 7,
    ResultBadCommand = 8,
    ResultInternal = 9,
    ResultNoFolder = 10,
    ResultChecksumMismatch = 11,
};

// DRIVE.media (§ 4.5).
enum Media : quint8 { MediaUnknown = 0, MediaSsd = 1, MediaSpinning = 2 };

// DRIVE.verdict (§ 4.5).
enum Verdict : quint8 { VerdictReportOnly = 1, VerdictReportAndDefrag = 2 };

// DRIVE.reason (§ 4.7), in the order the engine picks one.
enum Reason : quint16 {
    ReasonNone = 0,
    ReasonFsNotExt4 = 1,
    ReasonReadOnly = 2,
    ReasonSsd = 3,
    ReasonMediaUnknown = 4,
    ReasonOldCpu = 5,
};

// FILE_SKIPPED.stage (§ 4.6; 6 to 11 in a defrag, § 4.12).
enum SkipStage : quint16 {
    StageOpen = 1,
    StageStat = 2,
    StageExtents = 3,
    StageReadFolder = 4,
    StagePathTooLong = 5,
    StageRefused = 6,
    StageRead = 7,
    StageDonor = 8,
    StageMove = 9,
    StageChanged = 10,
    StageChecksumDiffers = 11,
};

// MAP_RUNS.kind (§ 4.6).
enum MapKind : quint32 {
    MapFree = 1,
    MapFilesystem = 2,
    MapWholeFile = 3,
    MapFragmentedFile = 4,
};

// Enumerated fields stay plain integers: a value this window does not
// know decodes as itself, never as an error (§ 4.2, INV-9).

struct Hello {
    quint16 major = 0;
    quint16 minor = 0;
    quint32 command = 0;
    quint32 blockSize = 0;
    bool operator==(const Hello &) const = default;
};

struct Finished {
    qint32 result = 0;
    qint32 errnum = 0;
    bool operator==(const Finished &) const = default;
};

struct Drive {
    quint64 totalBlocks = 0;
    quint64 freeBlocks = 0;
    quint32 blockSize = 0;
    quint32 devMajor = 0;
    quint32 devMinor = 0;
    quint8 media = 0;
    quint8 verdict = 0;
    quint16 reason = 0;
    quint32 mountFlags = 0;
    // Raw bytes as the kernel gave them. Show a path with
    // model::messages::path; send it to the engine with
    // EngineProcess::pathArgument.
    QByteArray mountPoint;
    QByteArray source;
    QByteArray fsType;
    bool operator==(const Drive &) const = default;
};

struct FragmentedFile {
    quint32 extents = 0;
    quint64 sizeBlocks = 0;
    QByteArray path;  // relative to the mount point
    bool operator==(const FragmentedFile &) const = default;
};

struct FileSkipped {
    qint32 errnum = 0;
    quint16 stage = 0;
    QByteArray path;  // relative to the mount point
    bool operator==(const FileSkipped &) const = default;
};

struct ReportSummary {
    quint64 files = 0;
    quint64 fragmentedFiles = 0;
    quint64 extents = 0;
    quint64 fileBlocks = 0;
    quint64 skipped = 0;
    bool operator==(const ReportSummary &) const = default;
};

struct ReportProgress {
    quint64 names = 0;     // names the walk has read so far
    quint64 estimate = 0;  // the drive's used inodes; 0 when it has no count
    bool operator==(const ReportProgress &) const = default;
};

// Blocks start to start + length - 1, in HELLO.blockSize units.
struct MapRun {
    quint64 start = 0;
    quint64 length = 0;
    bool operator==(const MapRun &) const = default;
};

struct MapRuns {
    quint32 kind = 0;
    QList<MapRun> runs;
    bool operator==(const MapRuns &) const = default;
};

struct RunState {
    quint32 state = 0;
    bool operator==(const RunState &) const = default;
};

struct FileDefragged {
    quint32 extentsBefore = 0;
    quint32 extentsAfter = 0;
    quint64 movedBlocks = 0;  // 0: no free space held it in fewer pieces; left as it was
    QByteArray path;          // relative to the mount point
    bool operator==(const FileDefragged &) const = default;
};

// One move: block k of `from` went to block k of `to`.
struct BlocksMoved {
    QList<MapRun> from;
    QList<MapRun> to;
    bool operator==(const BlocksMoved &) const = default;
};

// A record of a type this window does not know; its payload is skipped.
struct Unknown {
    quint16 type = 0;
    bool operator==(const Unknown &) const = default;
};

using Record = std::variant<Hello, Finished, Drive, FragmentedFile, FileSkipped,
                            ReportSummary, ReportProgress, MapRuns, RunState,
                            FileDefragged, BlocksMoved, Unknown>;

enum class StreamError { Malformed, VersionMismatch };

class RecordDecoder {
public:
    // Appends bytes; decodes every complete record they finish.
    void feed(QByteArrayView bytes);
    // Decoded records not yet taken, in stream order.
    [[nodiscard]] QList<Record> takeRecords();
    // Input has ended; runs the end-of-stream checks.
    void finish();
    // Set once the stream is malformed or a version mismatch; sticky.
    std::optional<StreamError> error() const { return m_error; }

private:
    // Decodes one framed record; false when it broke a rule.
    bool decode(quint16 type, QByteArrayView payload);
    // The stream-order rules for a record about to be accepted.
    bool orderAllows(const Record &record);

    QByteArray m_pending;  // bytes of a record not yet complete
    QList<Record> m_records;
    std::optional<StreamError> m_error;

    bool m_seenHello = false;
    bool m_seenFinished = false;
    quint32 m_command = 0;
    quint32 m_blockSize = 0;
    QList<QByteArray> m_mountPoints;  // probe: no two DRIVEs share one
    quint64 m_fragmented = 0;
    quint64 m_skipped = 0;
    std::optional<ReportSummary> m_summary;
};

}  // namespace defragr::engine
