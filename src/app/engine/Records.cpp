// The record decoder: framing (§ 4.1), versioning (§ 4.2), each record's
// fields (§ 4.4 to § 4.6) and the stream-order rules (§ 4.3, § 4.10) of
// docs/specs/DFR-0002-engine-protocol.md.
#include "engine/Records.h"

#include <QtEndian>

#include <utility>

namespace defragr::engine {

namespace {

// Fixed-part sizes, from each record's table in the spec.
constexpr quint32 kHelloSize = 16;
constexpr quint32 kFinishedSize = 8;
constexpr quint32 kDriveSize = 40;
constexpr quint32 kFragmentedFileSize = 16;
constexpr quint32 kFileSkippedSize = 8;
constexpr quint32 kReportSummarySize = 40;
constexpr quint32 kReportProgressSize = 16;
constexpr quint32 kRunStateSize = 8;

// Reads one payload: fixed fields by offset, then strings in order.
class PayloadReader {
public:
    explicit PayloadReader(QByteArrayView payload) : m_payload(payload) {}

    template <typename T>
    T at(qsizetype offset) const
    {
        return qFromLittleEndian<T>(m_payload.data() + offset);
    }

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

private:
    QByteArrayView m_payload;
    qsizetype m_cursor = 0;
};

quint32 fixedSize(quint16 type)
{
    switch (type) {
    case TypeHello: return kHelloSize;
    case TypeFinished: return kFinishedSize;
    case TypeDrive: return kDriveSize;
    case TypeFragmentedFile: return kFragmentedFileSize;
    case TypeFileSkipped: return kFileSkippedSize;
    case TypeReportSummary: return kReportSummarySize;
    case TypeReportProgress: return kReportProgressSize;
    case TypeRunState: return kRunStateSize;
    default: return 0;
    }
}

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
    if (payload.size() < qsizetype(fixedSize(type))) {
        m_error = StreamError::Malformed;
        return false;
    }
    PayloadReader in(payload);
    Record record;
    switch (type) {
    case TypeHello:
        record = Hello{in.at<quint16>(0), in.at<quint16>(2), in.at<quint32>(4),
                       in.at<quint32>(8)};
        break;
    case TypeFinished:
        record = Finished{in.at<qint32>(0), in.at<qint32>(4)};
        break;
    case TypeDrive: {
        Drive d{in.at<quint64>(0), in.at<quint64>(8), in.at<quint32>(16),
                in.at<quint32>(20), in.at<quint32>(24), in.at<quint8>(28),
                in.at<quint8>(29), in.at<quint16>(30), in.at<quint32>(32),
                {}, {}, {}};
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
        FragmentedFile f{in.at<quint32>(0), in.at<quint64>(8), {}};
        in.startStringsAt(kFragmentedFileSize);
        if (!in.string(f.path)) {
            m_error = StreamError::Malformed;
            return false;
        }
        record = std::move(f);
        break;
    }
    case TypeFileSkipped: {
        FileSkipped s{in.at<qint32>(0), in.at<quint16>(4), {}};
        in.startStringsAt(kFileSkippedSize);
        if (!in.string(s.path)) {
            m_error = StreamError::Malformed;
            return false;
        }
        record = std::move(s);
        break;
    }
    case TypeReportSummary:
        record = ReportSummary{in.at<quint64>(0), in.at<quint64>(8), in.at<quint64>(16),
                               in.at<quint64>(24), in.at<quint64>(32)};
        break;
    case TypeReportProgress:
        record = ReportProgress{in.at<quint64>(0), in.at<quint64>(8)};
        break;
    case TypeRunState:
        record = RunState{in.at<quint32>(0)};
        break;
    default:
        record = Unknown{type};
        break;
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
        if (m_seenHello)
            return malformed();
        if (hello->major != kMajor) {
            m_error = StreamError::VersionMismatch;
            return false;
        }
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
        // § 4.6 and INV-8: files, skips, progress and run states, then the summary
        // only on OK, immediately before FINISHED, agreeing with the counts.
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
            || std::holds_alternative<RunState>(record))
            return true;
        return malformed();
    }

    // Any other command: only § 4.3's rules.
    return true;
}

}  // namespace defragr::engine
