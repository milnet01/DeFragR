// The built engine against the window's decoder: INV-10, INV-11 and INV-18 of
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
        QCOMPARE(run.records.size(), 13);

        QCOMPARE(std::get<Hello>(run.records[0]), (Hello{1, 6, 255, 0x01020304}));

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
        QCOMPARE(std::get<ReportProgress>(run.records[5]),
                 (ReportProgress{0xE1E2E3E4E5E6E7E8, 0xF1F2F3F4F5F6F7F8}));
        QCOMPARE(std::get<MapRuns>(run.records[6]),
                 (MapRuns{0x13141516,
                          {{0x3132333435363738, 0x4142434445464748},
                           {0x5152535455565758, 0x6162636465666768}}}));
        QCOMPARE(std::get<RunState>(run.records[7]), (RunState{0xF1F2F3F4}));
        QCOMPARE(std::get<FileDefragged>(run.records[8]),
                 (FileDefragged{0x21222324, 0x25262728, 0x2A2B2C2D2E2F3031, "d/e"}));
        QCOMPARE(std::get<BlocksMoved>(run.records[9]),
                 (BlocksMoved{{{0x7172737475767778, 0x8182838485868788}},
                              {{0x9192939495969798, 0xA1A2A3A4A5A6A7A8},
                               {0xB1B2B3B4B5B6B7B8, 0xC1C2C3C4C5C6C7C8}}}));
        QCOMPARE(std::get<MapDone>(run.records[10]), MapDone{});
        QCOMPARE(std::get<DefragProgress>(run.records[11]),
                 (DefragProgress{0xD1D2D3D4D5D6D7D8, 0xD9DADBDCDDDEDFE0, 0xC9CACBCCCDCECFD0,
                                 0xB9BABBBCBDBEBFC0}));
        QCOMPARE(std::get<Finished>(run.records[12]), (Finished{0, 0}));
    }

    // INV-18: the engine's CRC32C gives every published value § 4.11 lists.
    void selftestCrc32c()
    {
        const Run run = runEngine({QStringLiteral("selftest-crc32c")});
        QCOMPARE(run.error, std::nullopt);
        QCOMPARE(run.records.size(), 2);
        QCOMPARE(std::get<Hello>(run.records[0]), (Hello{1, 6, CommandSelftestCrc32c, 0}));
        QCOMPARE(std::get<Finished>(run.records[1]).result, int(ResultOk));
        QCOMPARE(run.exitCode, 0);
    }

    // INV-11: every run is a clean stream whose result is the exit status.
    void everyRunFinishes_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::addColumn<int>("result");
        QTest::addColumn<bool>("needsNonRoot");
        QTest::newRow("no arguments") << QStringList{} << int(ResultUsage) << false;
        QTest::newRow("unknown command") << QStringList{"bogus"} << int(ResultUsage) << false;
        QTest::newRow("probe") << QStringList{"probe"} << int(ResultOk) << false;
        QTest::newRow("probe, extra argument") << QStringList{"probe", "x"} << int(ResultUsage)
                                               << false;
        // Arguments are checked before admin rights...
        QTest::newRow("report, no mount point") << QStringList{"report"} << int(ResultUsage) << false;
        // ...and admin rights before the mount point.
        QTest::newRow("report, not root")
            << QStringList{"report", "/nonexistent"} << int(ResultNotRoot) << true;
        // INV-21: defrag checks its arguments, then admin rights.
        QTest::newRow("defrag, no folder")
            << QStringList{"defrag", "/nonexistent"} << int(ResultUsage) << false;
        QTest::newRow("defrag, not root")
            << QStringList{"defrag", "/nonexistent", "x"} << int(ResultNotRoot) << true;
        // INV-25: a fourth word must be "map", and nothing follows it.
        QTest::newRow("defrag with the map, not root")
            << QStringList{"defrag", "/nonexistent", "x", "map"} << int(ResultNotRoot) << true;
        QTest::newRow("defrag, a fourth word that is not map")
            << QStringList{"defrag", "/nonexistent", "x", "maps"} << int(ResultUsage) << false;
        QTest::newRow("defrag, a word after map")
            << QStringList{"defrag", "/nonexistent", "x", "map", "y"} << int(ResultUsage) << false;
        QTest::newRow("selftest-records") << QStringList{"selftest-records"} << int(ResultOk)
                                          << false;
        QTest::newRow("selftest-crc32c, extra argument")
            << QStringList{"selftest-crc32c", "x"} << int(ResultUsage) << false;
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

    // § 6: a write that fails with an error other than EPIPE ends the run
    // at once with INTERNAL, and the engine prints nothing about it.
    // /dev/full fails every write with ENOSPC, the first one included.
    void failedWriteIsInternal_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::newRow("unknown command") << QStringList{"bogus"};
        QTest::newRow("selftest-records") << QStringList{"selftest-records"};
    }

    void failedWriteIsInternal()
    {
        QFETCH(QStringList, args);
        QProcess engine;
        engine.setStandardOutputFile(QStringLiteral("/dev/full"));
        engine.start(qEnvironmentVariable("DEFRAGR_ENGINE"), args);
        QVERIFY2(engine.waitForStarted(10000), qPrintable(engine.errorString()));
        engine.closeWriteChannel();
        QVERIFY(engine.waitForFinished(10000));
        QCOMPARE(engine.exitStatus(), QProcess::NormalExit);
        QCOMPARE(engine.exitCode(), int(ResultInternal));
        QCOMPARE(engine.readAllStandardError(), QByteArray());
    }

    // § 6, the window dies: standard output is a pipe with no reader, so
    // the first write fails with EPIPE, and the engine exits CANCELLED
    // rather than dying of SIGPIPE.
    void closedPipeIsCancelled()
    {
        int fds[2];
        QCOMPARE(pipe(fds), 0);
        ::close(fds[0]);
        QProcess engine;
        engine.setChildProcessModifier([&fds] { ::dup2(fds[1], STDOUT_FILENO); });
        engine.start(qEnvironmentVariable("DEFRAGR_ENGINE"), {QStringLiteral("selftest-records")});
        const bool started = engine.waitForStarted(10000);
        ::close(fds[1]);
        QVERIFY2(started, qPrintable(engine.errorString()));
        engine.closeWriteChannel();
        QVERIFY(engine.waitForFinished(10000));
        QCOMPARE(engine.exitStatus(), QProcess::NormalExit);
        QCOMPARE(engine.exitCode(), int(ResultCancelled));
        QCOMPARE(engine.readAllStandardError(), QByteArray());
    }
};

QTEST_GUILESS_MAIN(TestRecordsConformance)
#include "tst_records_conformance.moc"
