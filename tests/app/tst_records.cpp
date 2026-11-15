// The record decoder alone, against hand-built streams: INV-1 to INV-9 of
// docs/specs/DFR-0002-engine-protocol.md. Proves nothing about the engine;
// tst_records_conformance.cpp reads real engine output.
#include "engine/Records.h"

#include <QTest>
#include <QtEndian>

using namespace defragr::engine;

namespace {

// ── Building streams ────────────────────────────────────────────────────

void put16(QByteArray &out, quint16 v)
{
    char b[2];
    qToLittleEndian(v, b);
    out.append(b, 2);
}
void put32(QByteArray &out, quint32 v)
{
    char b[4];
    qToLittleEndian(v, b);
    out.append(b, 4);
}
void put64(QByteArray &out, quint64 v)
{
    char b[8];
    qToLittleEndian(v, b);
    out.append(b, 8);
}
// A string as § 4.1 frames it, with `count` written in place of the real
// byte count when given.
void putString(QByteArray &payload, const QByteArray &bytes,
               std::optional<quint32> count = std::nullopt)
{
    put32(payload, count.value_or(quint32(bytes.size())));
    payload.append(bytes);
    while (payload.size() % 8 != 0)
        payload.append('\0');
}

// A header with any type and length, then the payload as given.
QByteArray frame(quint16 type, quint32 length, const QByteArray &payload)
{
    QByteArray out;
    put16(out, type);
    put16(out, 0);
    put32(out, length);
    return out + payload;
}
QByteArray record(quint16 type, const QByteArray &payload)
{
    return frame(type, quint32(payload.size()), payload);
}

QByteArray hello(quint32 command, quint32 blockSize = 4096, quint16 major = 1,
                 quint16 minor = 0)
{
    QByteArray p;
    put16(p, major);
    put16(p, minor);
    put32(p, command);
    put32(p, blockSize);
    put32(p, 0);
    return record(TypeHello, p);
}
QByteArray finished(qint32 result = ResultOk, qint32 errnum = 0)
{
    QByteArray p;
    put32(p, quint32(result));
    put32(p, quint32(errnum));
    return record(TypeFinished, p);
}
QByteArray drivePayload(quint8 verdict, quint16 reason, const QByteArray &mountPoint = "/",
                        quint8 media = 2)
{
    QByteArray p;
    put64(p, 1000);
    put64(p, 500);
    put32(p, 4096);
    put32(p, 8);
    put32(p, 1);
    p.append(char(media));
    p.append(char(verdict));
    put16(p, reason);
    put32(p, 0);
    put32(p, 0);
    putString(p, mountPoint);
    putString(p, "/dev/sda1");
    putString(p, "ext4");
    return p;
}
QByteArray drive(quint8 verdict, quint16 reason, const QByteArray &mountPoint = "/")
{
    return record(TypeDrive, drivePayload(verdict, reason, mountPoint));
}
QByteArray fragmented(const QByteArray &path)
{
    QByteArray p;
    put32(p, 3);
    put32(p, 0);
    put64(p, 12);
    putString(p, path);
    return record(TypeFragmentedFile, p);
}
QByteArray skippedFile(const QByteArray &path, quint16 stage = 1)
{
    QByteArray p;
    put32(p, 13);
    put16(p, stage);
    put16(p, 0);
    putString(p, path);
    return record(TypeFileSkipped, p);
}
QByteArray summary(quint64 fragmentedFiles, quint64 skipped)
{
    QByteArray p;
    put64(p, 10);
    put64(p, fragmentedFiles);
    put64(p, 20);
    put64(p, 30);
    put64(p, skipped);
    return record(TypeReportSummary, p);
}
QByteArray runState(quint32 state)
{
    QByteArray p;
    put32(p, state);
    put32(p, 0);
    return record(TypeRunState, p);
}
QByteArray unknown(quint16 type, quint32 length)
{
    return record(type, QByteArray(qsizetype(length), '\x5A'));
}

// The selftest-records stream, built from § 4.9's table.
QByteArray selftestStream()
{
    QByteArray drivePart;
    put64(drivePart, 0x1112131415161718);
    put64(drivePart, 0x2122232425262728);
    put32(drivePart, 0x31323334);
    put32(drivePart, 0x41424344);
    put32(drivePart, 0x51525354);
    drivePart.append(char(2));
    drivePart.append(char(1));
    put16(drivePart, 0xD1D2);
    put32(drivePart, 0xE1E2E3E5);
    put32(drivePart, 0);
    putString(drivePart, QByteArray("/mnt/t\xFF", 7));
    putString(drivePart, "");
    putString(drivePart, "ext4");

    QByteArray frag;
    put32(frag, 0x61626364);
    put32(frag, 0);
    put64(frag, 0x7172737475767778);
    putString(frag, "a/b");

    QByteArray skip;
    put32(skip, 0x0A0B0C0D);
    put16(skip, 0x0E0F);
    put16(skip, 0);
    putString(skip, "c");

    QByteArray sum;
    put64(sum, 0x8182838485868788);
    put64(sum, 0x9192939495969798);
    put64(sum, 0xA1A2A3A4A5A6A7A8);
    put64(sum, 0xB1B2B3B4B5B6B7B8);
    put64(sum, 0xC1C2C3C4C5C6C7C8);

    return hello(CommandSelftest, 0x01020304) + record(TypeDrive, drivePart)
         + record(TypeFragmentedFile, frag) + record(TypeFileSkipped, skip)
         + record(TypeReportSummary, sum) + runState(0xF1F2F3F4) + finished();
}

// ── Decoding ────────────────────────────────────────────────────────────

struct Outcome {
    QList<Record> records;
    std::optional<StreamError> error;
};

// Feeds `bytes` in pieces of at most `chunk` bytes (0: all at once).
Outcome decode(const QByteArray &bytes, qsizetype chunk = 0)
{
    RecordDecoder d;
    if (chunk == 0) {
        d.feed(bytes);
    } else {
        for (qsizetype i = 0; i < bytes.size(); i += chunk)
            d.feed(QByteArrayView(bytes).sliced(i, qMin(chunk, bytes.size() - i)));
    }
    d.finish();
    return {d.takeRecords(), d.error()};
}

std::optional<StreamError> errorOf(const QByteArray &bytes)
{
    return decode(bytes).error;
}

constexpr std::optional<StreamError> kNoError = std::nullopt;
constexpr std::optional<StreamError> kMalformed = StreamError::Malformed;
constexpr std::optional<StreamError> kMismatch = StreamError::VersionMismatch;

// A valid report stream around `body`: OK, with a summary of the counts.
QByteArray reportStream(const QByteArray &body, quint64 fragmentedFiles = 0,
                        quint64 skipped = 0)
{
    return hello(CommandReport) + body + summary(fragmentedFiles, skipped) + finished();
}

}  // namespace

class TestRecords : public QObject {
    Q_OBJECT

private slots:
    // INV-1
    void framing()
    {
        const QByteArray head = hello(CommandUnknown);
        const QByteArray tail = finished();
        const QByteArray payload(16, '\x5A');
        QCOMPARE(errorOf(head + frame(0x00FF, 16, payload) + tail), kNoError);
        QCOMPARE(errorOf(head + frame(0x0000, 16, payload) + tail), kMalformed);
        // Read as 12 bytes, this would land on FINISHED and pass.
        QCOMPARE(errorOf(head + frame(0x00FF, 12, QByteArray(12, '\x5A')) + tail), kMalformed);
        QCOMPARE(errorOf(head + frame(0x00FF, 65544, QByteArray(65544, '\x5A')) + tail),
                 kMalformed);
        QCOMPARE(errorOf(head + unknown(0x00FF, 0) + tail), kNoError);
        QCOMPARE(errorOf(head + unknown(0x00FF, 65536) + tail), kNoError);
    }

    // INV-2
    void unknownTypeSkipped()
    {
        const Outcome out = decode(reportStream(unknown(0x00FF, 16)));
        QCOMPARE(out.error, kNoError);
        QCOMPARE(out.records.size(), 4);
        QCOMPARE(std::get<Unknown>(out.records[1]).type, quint16(0x00FF));
        QCOMPARE(std::get<ReportSummary>(out.records[2]).files, quint64(10));
        QVERIFY(std::holds_alternative<Finished>(out.records[3]));
    }

    // INV-3
    void payloadLength()
    {
        const QByteArray head = hello(CommandUnknown);
        QCOMPARE(errorOf(head + record(TypeFinished, {})), kMalformed);

        QByteArray longer;
        put32(longer, quint32(ResultUsage));
        put32(longer, 22);
        longer.append(QByteArray(8, '\x77'));
        const Outcome out = decode(head + record(TypeFinished, longer));
        QCOMPARE(out.error, kNoError);
        QCOMPARE(std::get<Finished>(out.records.last()), (Finished{ResultUsage, 22}));
    }

    // INV-4
    void strings()
    {
        const QByteArray mount("/mnt/t\xFF", 7);
        const Outcome out = decode(hello(CommandProbe, 0)
                                   + record(TypeDrive, drivePayload(1, 3, mount)) + finished());
        QCOMPARE(out.error, kNoError);
        const auto &d = std::get<Drive>(out.records[1]);
        QCOMPARE(d.mountPoint, mount);
        QCOMPARE(d.source, QByteArray("/dev/sda1"));
        QCOMPARE(d.fsType, QByteArray("ext4"));

        // An empty string, and one of exactly the limit.
        QByteArray edge = drivePayload(1, 3).first(40);
        putString(edge, QByteArray(4096, 'x'));
        putString(edge, "");
        putString(edge, "ext4");
        const Outcome edgeOut = decode(hello(CommandProbe, 0) + record(TypeDrive, edge) + finished());
        QCOMPARE(edgeOut.error, kNoError);
        QCOMPARE(std::get<Drive>(edgeOut.records[1]).mountPoint.size(), 4096);
        QVERIFY(std::get<Drive>(edgeOut.records[1]).source.isEmpty());

        // Counts over the limit, or past the payload's end; the fixed part
        // and every byte the count claims are present otherwise.
        QByteArray tooLong = drivePayload(1, 3).first(40);
        putString(tooLong, QByteArray(4097, 'x'));
        putString(tooLong, "");
        putString(tooLong, "ext4");
        QCOMPARE(errorOf(hello(CommandProbe, 0) + record(TypeDrive, tooLong) + finished()),
                 kMalformed);

        QByteArray pastEnd = drivePayload(1, 3).first(40);
        putString(pastEnd, "/");
        putString(pastEnd, "/dev/sda1");
        putString(pastEnd, "ext4", 16);  // claims 16 bytes; 4 remain
        QCOMPARE(errorOf(hello(CommandProbe, 0) + record(TypeDrive, pastEnd) + finished()),
                 kMalformed);
    }

    // INV-5
    void chunking()
    {
        const QByteArray good = selftestStream();
        QByteArray bad = good;
        bad[bad.size() - 16] = 0;  // FINISHED's type becomes 0
        bad[bad.size() - 15] = 0;
        for (const QByteArray &stream : {good, bad}) {
            const Outcome whole = decode(stream);
            QVERIFY(!whole.records.isEmpty());
            const Outcome bytewise = decode(stream, 1);
            QCOMPARE(bytewise.records, whole.records);
            QCOMPARE(bytewise.error, whole.error);
            for (qsizetype split = 1; split < stream.size(); ++split) {
                RecordDecoder d;
                d.feed(QByteArrayView(stream).first(split));
                d.feed(QByteArrayView(stream).sliced(split));
                d.finish();
                QCOMPARE(d.takeRecords(), whole.records);
                QCOMPARE(d.error(), whole.error);
            }
        }
        QCOMPARE(decode(good).error, kNoError);
        QCOMPARE(decode(bad).error, kMalformed);
    }

    // INV-6
    void streamOrder()
    {
        const QByteArray h = hello(CommandUnknown);
        QCOMPARE(errorOf(h + finished()), kNoError);
        QCOMPARE(errorOf(runState(1) + h + finished()), kMalformed);
        QCOMPARE(errorOf(unknown(0x00FF, 8) + h + finished()), kMalformed);
        QCOMPARE(errorOf(h + h + finished()), kMalformed);
        QCOMPARE(errorOf(h + finished() + finished()), kMalformed);
        QCOMPARE(errorOf(h + finished() + unknown(0x00FF, 8)), kMalformed);
        QCOMPARE(errorOf(h + runState(1)), kMalformed);
        QCOMPARE(errorOf(h + finished() + QByteArray(4, '\0')), kMalformed);
        QCOMPARE(errorOf({}), kMalformed);

        // § 4.5: a probe carries DRIVE records only, each mount point once.
        const QByteArray p = hello(CommandProbe, 0);
        QCOMPARE(errorOf(p + drive(1, 3, "/") + drive(1, 3, "/home") + finished()), kNoError);
        QCOMPARE(errorOf(p + drive(1, 3, "/") + drive(1, 3, "/") + finished()), kMalformed);
        QCOMPARE(errorOf(p + fragmented("a") + finished()), kMalformed);
        // § 4.6: a report carries no DRIVE.
        QCOMPARE(errorOf(reportStream(drive(1, 3))), kMalformed);
    }

    // INV-7
    void version()
    {
        QCOMPARE(errorOf(hello(CommandUnknown, 0, 2) + finished()), kMismatch);
        QCOMPARE(errorOf(hello(CommandUnknown, 0, 0) + finished()), kMismatch);
        QCOMPARE(errorOf(hello(CommandUnknown, 0, 1, 7) + finished()), kNoError);
    }

    // INV-8
    void summaryCounts()
    {
        const QByteArray two = fragmented("a") + fragmented("b");
        QCOMPARE(errorOf(reportStream(two, 2)), kNoError);
        QCOMPARE(errorOf(reportStream(two, 3)), kMalformed);
        QCOMPARE(errorOf(reportStream(skippedFile("c"), 0, 1)), kNoError);
        QCOMPARE(errorOf(reportStream(skippedFile("c"), 0, 0)), kMalformed);
        QCOMPARE(errorOf(reportStream(runState(2) + runState(1))), kNoError);

        const QByteArray h = hello(CommandReport);
        QCOMPARE(errorOf(hello(CommandReport, 0) + summary(0, 0) + finished()), kMalformed);
        QCOMPARE(errorOf(h + finished()), kMalformed);
        QCOMPARE(errorOf(h + summary(0, 0) + summary(0, 0) + finished()), kMalformed);
        QCOMPARE(errorOf(h + summary(0, 0) + fragmented("a") + finished()), kMalformed);
        QCOMPARE(errorOf(h + summary(0, 0) + unknown(0x00FF, 8) + finished()), kNoError);
        QCOMPARE(errorOf(h + summary(0, 0) + finished(ResultCancelled)), kMalformed);
        QCOMPARE(errorOf(h + fragmented("a") + finished(ResultCancelled)), kNoError);
    }

    // INV-9
    void enumerations()
    {
        const QByteArray p = hello(CommandProbe, 0);
        QCOMPARE(errorOf(p + drive(1, 200) + finished()), kNoError);
        QCOMPARE(errorOf(p + drive(3, 0) + finished()), kNoError);
        QCOMPARE(errorOf(p + drive(3, 5) + finished()), kNoError);
        QCOMPARE(errorOf(p + drive(2, 0) + finished()), kNoError);
        QCOMPARE(errorOf(p + drive(2, 1) + finished()), kMalformed);
        QCOMPARE(errorOf(p + drive(1, 0) + finished()), kMalformed);
        QCOMPARE(errorOf(p + record(TypeDrive, drivePayload(1, 3, "/", 77)) + finished()),
                 kNoError);

        QCOMPARE(errorOf(hello(77) + finished(300)), kNoError);
        QCOMPARE(errorOf(reportStream(skippedFile("c", 99) + runState(50), 0, 1)), kNoError);
    }
};

QTEST_GUILESS_MAIN(TestRecords)
#include "tst_records.moc"
