// The record decoder: framing (§ 4.1), versioning (§ 4.2), each record's
// fields (§ 4.4 to § 4.6, § 4.12) and the stream-order rules (§ 4.3,
// § 4.10) of
// docs/specs/DFR-0002-engine-protocol.md.
#include "engine/Records.h"

#include <QtEndian>

#include <utility>

namespace defragr::engine {

namespace {

// Where each record's strings or runs start: its fixed part's size, from
// its table in the spec.
constexpr quint32 kDriveSize = 40;
constexpr quint32 kFragmentedFileSize = 16;
constexpr quint32 kFileSkippedSize = 8;
constexpr quint32 kMapRunsSize = 8;
constexpr quint32 kMapRunSize = 16;  // each run after MAP_RUNS' fixed part
constexpr quint32 kFileDefraggedSize = 16;
constexpr quint32 kBlocksMovedSize = 8;  // its runs are laid out as MAP_RUNS'

// Reads one payload: fixed fields by offset, then strings in order. A
// field past the payload's end reads as 0 and marks the payload short,
// so no type needs its size listed twice.
class PayloadReader {
public:
    explicit PayloadReader(QByteArrayView payload) : m_payload(payload) {}

    template <typename T>
    T at(qsizetype offset)
    {
        if (offset + qsizetype(sizeof(T)) > m_payload.size()) {
            m_short = true;
            return T{};
        }
        return qFromLittleEndian<T>(m_payload.data() + offset);
    }

    // A field was read past the payload's end.
    bool isShort() const { return m_short; }

    // The next string after `m_cursor`; false when its count is over the
    // limit or it runs past the payload (§ 4.1).
    bool string(QByteArray &out)
    {
        if (m_cursor + 4 > m_payload.size())
            return false;
        const quint32 count = at<quint32>(m_cursor);
        if (count > kStringMax || m_cursor + 4 + qsizetype(count) > m_payload.size())
            return false;
        out = m_payload.sliced(m_cursor + 4, count).toByteArray();
        m_cursor = (m_cursor + 4 + count + 7) & ~qsizetype(7);
        return true;
    }

    void startStringsAt(qsizetype offset) { m_cursor = offset; }

    // `count` runs from `offset`; false when they pass the payload's end.
    bool runs(qsizetype offset, quint32 count, QList<MapRun> &out)
    {
        if (quint64(offset) + quint64(count) * kMapRunSize > quint64(m_payload.size()))
            return false;
        out.reserve(count);
        for (quint32 i = 0; i < count; ++i) {
            const qsizetype at = offset + qsizetype(i) * kMapRunSize;
            out.append({.start = this->at<quint64>(at), .length = this->at<quint64>(at + 8)});
        }
        return true;
    }

private:
    QByteArrayView m_payload;
    qsizetype m_cursor = 0;
    bool m_short = false;
};

}  // namespace

void RecordDecoder::feed(QByteArrayView bytes)
{
    if (m_error)
        return;
    m_pending.append(bytes);

    qsizetype offset = 0;
    while (m_pending.size() - offset >= kHeaderSize) {
        const char *header = m_pending.constData() + offset;
        const quint16 type = qFromLittleEndian<quint16>(header);
        const quint32 length = qFromLittleEndian<quint32>(header + 4);
        if (type == 0 || length % 8 != 0 || length > kPayloadMax) {
            m_error = StreamError::Malformed;
            break;
        }
        if (m_pending.size() - offset - kHeaderSize < qsizetype(length))
            break;  // the payload has not all arrived
        const QByteArrayView payload(header + kHeaderSize, length);
        offset += kHeaderSize + length;
        if (!decode(type, payload))
            break;
    }
    m_pending.remove(0, offset);
}

QList<Record> RecordDecoder::takeRecords()
{
    return std::exchange(m_records, {});
}

void RecordDecoder::finish()
{
    if (m_error)
        return;
    // A partial record, or no FINISHED, is a cut-off stream (INV-6).
    if (!m_pending.isEmpty() || !m_seenFinished)
        m_error = StreamError::Malformed;
}

bool RecordDecoder::decode(quint16 type, QByteArrayView payload)
{
    // Another major may lay HELLO out differently; only its first field is
    // certain, so it is read before the rest (§ 4.2).
    if (type == TypeHello && !m_seenHello && payload.size() >= 2
        && qFromLittleEndian<quint16>(payload.data()) != kMajor) {
        m_error = StreamError::VersionMismatch;
        return false;
    }
    PayloadReader in(payload);
    Record record;
    switch (type) {
    case TypeHello:
        record = Hello{.major = in.at<quint16>(0),
                       .minor = in.at<quint16>(2),
                       .command = in.at<quint32>(4),
                       .blockSize = in.at<quint32>(8)};
        break;
    case TypeFinished:
        record = Finished{.result = in.at<qint32>(0), .errnum = in.at<qint32>(4)};
        break;
    case TypeDrive: {
        Drive d{.totalBlocks = in.at<quint64>(0),
                .freeBlocks = in.at<quint64>(8),
                .blockSize = in.at<quint32>(16),
                .devMajor = in.at<quint32>(20),
                .devMinor = in.at<quint32>(24),
                .media = in.at<quint8>(28),
                .verdict = in.at<quint8>(29),
                .reason = in.at<quint16>(30),
                .mountFlags = in.at<quint32>(32),
                .mountPoint = {},  // the strings follow the fixed part
                .source = {},
                .fsType = {}};
        in.startStringsAt(kDriveSize);
        if (!in.string(d.mountPoint) || !in.string(d.source) || !in.string(d.fsType)) {
            m_error = StreamError::Malformed;
            return false;
        }
        // A known verdict must agree with its reason (INV-9).
        if ((d.verdict == VerdictReportAndDefrag && d.reason != 0)
            || (d.verdict == VerdictReportOnly && d.reason == 0)) {
            m_error = StreamError::Malformed;
            return false;
        }
        record = std::move(d);
        break;
    }
    case TypeFragmentedFile: {
        FragmentedFile f{
            .extents = in.at<quint32>(0), .sizeBlocks = in.at<quint64>(8), .path = {}};
        in.startStringsAt(kFragmentedFileSize);
        if (!in.string(f.path)) {
            m_error = StreamError::Malformed;
            return false;
        }
        record = std::move(f);
        break;
    }
    case TypeFileSkipped: {
        FileSkipped s{.errnum = in.at<qint32>(0), .stage = in.at<quint16>(4), .path = {}};
        in.startStringsAt(kFileSkippedSize);
        if (!in.string(s.path)) {
            m_error = StreamError::Malformed;
            return false;
        }
        record = std::move(s);
        break;
    }
    case TypeReportSummary:
        record = ReportSummary{.files = in.at<quint64>(0),
                               .fragmentedFiles = in.at<quint64>(8),
                               .extents = in.at<quint64>(16),
                               .fileBlocks = in.at<quint64>(24),
                               .skipped = in.at<quint64>(32)};
        break;
    case TypeReportProgress:
        record = ReportProgress{.names = in.at<quint64>(0), .estimate = in.at<quint64>(8)};
        break;
    case TypeMapRuns: {
        MapRuns m{.kind = in.at<quint32>(0), .runs = {}};
        if (!in.runs(kMapRunsSize, in.at<quint32>(4), m.runs)) {
            m_error = StreamError::Malformed;
            return false;
        }
        record = std::move(m);
        break;
    }
    case TypeMapDone:
        in.at<quint64>(0);  // reserved; read so a shorter payload is short
        record = MapDone{};
        break;
    case TypeFileDefragged: {
        FileDefragged f{.extentsBefore = in.at<quint32>(0),
                        .extentsAfter = in.at<quint32>(4),
                        .movedBlocks = in.at<quint64>(8),
                        .path = {}};
        in.startStringsAt(kFileDefraggedSize);
        if (!in.string(f.path)) {
            m_error = StreamError::Malformed;
            return false;
        }
        record = std::move(f);
        break;
    }
    case TypeBlocksMoved: {
        const quint32 fromCount = in.at<quint32>(0);
        BlocksMoved b;
        if (!in.runs(kBlocksMovedSize, fromCount, b.from)
            || !in.runs(kBlocksMovedSize + qsizetype(fromCount) * kMapRunSize, in.at<quint32>(4),
                        b.to)) {
            m_error = StreamError::Malformed;
            return false;
        }
        record = std::move(b);
        break;
    }
    case TypeDefragProgress:
        record = DefragProgress{.filesDone = in.at<quint64>(0),
                                .filesTotal = in.at<quint64>(8),
                                .blocksDone = in.at<quint64>(16),
                                .blocksTotal = in.at<quint64>(24)};
        break;
    case TypeRunState:
        record = RunState{.state = in.at<quint32>(0)};
        break;
    default:
        record = Unknown{type};
        break;
    }
    if (in.isShort()) {
        m_error = StreamError::Malformed;
        return false;
    }

    if (!orderAllows(record))
        return false;
    m_records.append(std::move(record));
    return true;
}

bool RecordDecoder::orderAllows(const Record &record)
{
    const auto malformed = [this] {
        m_error = StreamError::Malformed;
        return false;
    };

    // § 4.3: HELLO first and once, FINISHED last and once.
    if (m_seenFinished)
        return malformed();
    if (const auto *hello = std::get_if<Hello>(&record)) {
        // Another major was refused in decode(), before the fixed part.
        if (m_seenHello)
            return malformed();
        m_seenHello = true;
        m_command = hello->command;
        m_blockSize = hello->blockSize;
        return true;
    }
    if (!m_seenHello)
        return malformed();

    // An Unknown may sit anywhere between HELLO and FINISHED (§ 4.10).
    if (std::holds_alternative<Unknown>(record))
        return true;

    const auto *finished = std::get_if<Finished>(&record);
    if (finished)
        m_seenFinished = true;

    if (m_command == CommandProbe) {
        // § 4.5: only DRIVE records, no two with one mount point.
        if (finished)
            return true;
        const auto *drive = std::get_if<Drive>(&record);
        if (!drive || m_mountPoints.contains(drive->mountPoint))
            return malformed();
        m_mountPoints.append(drive->mountPoint);
        return true;
    }

    if (m_command == CommandReport) {
        // § 4.6 and INV-8: files, skips, progress, map runs and run states, then
        // the summary only on OK, immediately before FINISHED, agreeing with the
        // counts.
        if (finished) {
            if (finished->result != ResultOk)
                return m_summary ? malformed() : true;
            if (!m_summary || m_blockSize == 0 || m_summary->fragmentedFiles != m_fragmented
                || m_summary->skipped != m_skipped)
                return malformed();
            return true;
        }
        if (m_summary)
            return malformed();
        if (const auto *summary = std::get_if<ReportSummary>(&record)) {
            m_summary = *summary;
            return true;
        }
        if (std::holds_alternative<FragmentedFile>(record)) {
            ++m_fragmented;
            return true;
        }
        if (std::holds_alternative<FileSkipped>(record)) {
            ++m_skipped;
            return true;
        }
        if (std::holds_alternative<ReportProgress>(record)
            || std::holds_alternative<MapRuns>(record)
            || std::holds_alternative<RunState>(record))
            return true;
        return malformed();
    }

    if (m_command == CommandDefrag) {
        // § 4.12, INV-19 and INV-24: first the map, when asked for, closed
        // by MAP_DONE; then progress, moves, files and run states. OK
        // needs the block size the runs are counted in.
        if (finished)
            return finished->result != ResultOk || m_blockSize != 0 ? true : malformed();
        if (std::holds_alternative<RunState>(record))
            return true;
        if (std::holds_alternative<MapRuns>(record)
            || std::holds_alternative<ReportProgress>(record)
            || std::holds_alternative<MapDone>(record)) {
            if (m_mapClosed)
                return malformed();
            m_mapClosed = std::holds_alternative<MapDone>(record);
            return true;
        }
        if (std::holds_alternative<FileDefragged>(record)
            || std::holds_alternative<FileSkipped>(record)
            || std::holds_alternative<BlocksMoved>(record)
            || std::holds_alternative<DefragProgress>(record)) {
            m_mapClosed = true;
            return true;
        }
        return malformed();
    }

    // Any other command: only § 4.3's rules.
    return true;
}

}  // namespace defragr::engine
