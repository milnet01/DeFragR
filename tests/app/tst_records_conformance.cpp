// The built engine against the window's decoder: INV-10 and INV-11 of
// docs/specs/DFR-0002-engine-protocol.md. The engine's path comes from
// DEFRAGR_ENGINE, which tests/app/CMakeLists.txt sets.
#include "engine/Records.h"

#include <QProcess>
#include <QTest>

#include <unistd.h>

using namespace defragr::engine;

namespace {

struct Run {
    QList<Record> records;
    std::optional<StreamError> error;
    int exitCode = -1;
};

Run runEngine(const QStringList &args)
{
    QProcess engine;
    engine.setProcessChannelMode(QProcess::SeparateChannels);
    engine.start(qEnvironmentVariable("DEFRAGR_ENGINE"), args);
    Run run;
    if (!engine.waitForStarted(10000)) {
        qWarning("engine did not start: %s", qPrintable(engine.errorString()));
        return run;
    }
    engine.closeWriteChannel();
    if (!engine.waitForFinished(10000) || engine.exitStatus() != QProcess::NormalExit) {
        qWarning("engine did not exit normally");
        return run;
    }
    RecordDecoder decoder;
    decoder.feed(engine.readAllStandardOutput());
    decoder.finish();
    run.records = decoder.takeRecords();
    run.error = decoder.error();
    run.exitCode = engine.exitCode();
    return run;
}

}  // namespace

class TestRecordsConformance : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(!qEnvironmentVariable("DEFRAGR_ENGINE").isEmpty(), "DEFRAGR_ENGINE is not set");
    }

    // INV-10: every field of every record, as § 4.9 fixes it.
    void selftestRecords()
    {
        const Run run = runEngine({QStringLiteral("selftest-records")});
        QCOMPARE(run.error, std::nullopt);
        QCOMPARE(run.exitCode, 0);
        QCOMPARE(run.records.size(), 7);

        QCOMPARE(std::get<Hello>(run.records[0]), (Hello{1, 0, 255, 0x01020304}));

        Drive drive;
        drive.totalBlocks = 0x1112131415161718;
        drive.freeBlocks = 0x2122232425262728;
        drive.blockSize = 0x31323334;
        drive.devMajor = 0x41424344;
        drive.devMinor = 0x51525354;
        drive.media = 2;
        drive.verdict = 1;
        drive.reason = 0xD1D2;
        drive.mountFlags = 0xE1E2E3E5;
        drive.mountPoint = QByteArray("/mnt/t\xFF", 7);
        drive.fsType = "ext4";
        QCOMPARE(std::get<Drive>(run.records[1]), drive);

        QCOMPARE(std::get<FragmentedFile>(run.records[2]),
                 (FragmentedFile{0x61626364, 0x7172737475767778, "a/b"}));
        QCOMPARE(std::get<FileSkipped>(run.records[3]),
                 (FileSkipped{0x0A0B0C0D, 0x0E0F, "c"}));
        QCOMPARE(std::get<ReportSummary>(run.records[4]),
                 (ReportSummary{0x8182838485868788, 0x9192939495969798, 0xA1A2A3A4A5A6A7A8,
                                0xB1B2B3B4B5B6B7B8, 0xC1C2C3C4C5C6C7C8}));
        QCOMPARE(std::get<RunState>(run.records[5]), (RunState{0xF1F2F3F4}));
        QCOMPARE(std::get<Finished>(run.records[6]), (Finished{0, 0}));
    }

    // INV-11: every run is a clean stream whose result is the exit status.
    void everyRunFinishes_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::addColumn<int>("result");
        QTest::addColumn<bool>("needsNonRoot");
        QTest::newRow("no arguments") << QStringList{} << int(ResultUsage) << false;
        QTest::newRow("unknown command") << QStringList{"bogus"} << int(ResultUsage) << false;
        // Arguments are checked before admin rights...
        QTest::newRow("report, no mount point") << QStringList{"report"} << int(ResultUsage) << false;
        // ...and admin rights before the mount point.
        QTest::newRow("report, not root")
            << QStringList{"report", "/nonexistent"} << int(ResultNotRoot) << true;
        QTest::newRow("selftest-records") << QStringList{"selftest-records"} << int(ResultOk)
                                          << false;
    }

    void everyRunFinishes()
    {
        QFETCH(QStringList, args);
        QFETCH(int, result);
        QFETCH(bool, needsNonRoot);
        if (needsNonRoot && geteuid() == 0)
            QSKIP("runs as root, so the admin-rights check passes");

        const Run run = runEngine(args);
        QCOMPARE(run.error, std::nullopt);
        QVERIFY(!run.records.isEmpty());
        const auto *finished = std::get_if<Finished>(&run.records.last());
        QVERIFY(finished);
        QCOMPARE(finished->result, result);
        QCOMPARE(run.exitCode, finished->result);
    }
};

QTEST_GUILESS_MAIN(TestRecordsConformance)
#include "tst_records_conformance.moc"
