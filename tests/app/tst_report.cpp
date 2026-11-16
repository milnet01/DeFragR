// Report: a drive's figures and its fragmented files worst first (S3),
// read from the fake engines in DEFRAGR_FAKES through a launcher that
// grants nothing. fakes/report.sh holds the report this test expects.
#include "model/Report.h"

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QTest>

using namespace defragr::engine;
using namespace defragr::model;

namespace {

struct Load {
    int loaded = 0;
    QList<Message> failures;
};

Load run(Report &report, const QByteArray &mountPoint)
{
    Load load;
    // Dies with this call, and takes both connections with it: a later
    // run must not reach this call's `load`.
    QObject listening;
    QObject::connect(&report, &Report::loaded, &listening, [&] { ++load.loaded; });
    QObject::connect(&report, &Report::failed, &listening,
                     [&](const Message &message) { load.failures.append(message); });
    report.run(mountPoint);
    if (!QTest::qWaitFor([&] { return load.loaded + load.failures.size() > 0; }, 10000))
        qWarning("the report did not end");
    return load;
}

ReportSummary summary(quint64 files, quint64 fragmented, quint64 extents, quint64 skipped = 0)
{
    ReportSummary made;
    made.files = files;
    made.fragmentedFiles = fragmented;
    made.extents = extents;
    made.skipped = skipped;
    return made;
}

}  // namespace

class TestReport : public QObject {
    Q_OBJECT

    QDir m_fakes;

    QString fake(const char *name) const { return m_fakes.filePath(QLatin1String(name)); }
    void useEngine(const char *name) { qputenv("DEFRAGR_ENGINE", QFile::encodeName(fake(name))); }

private slots:
    void initTestCase()
    {
        // The expected wording below is written in this locale's numbers.
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        m_fakes.setPath(qEnvironmentVariable("DEFRAGR_FAKES"));
        QVERIFY2(m_fakes.exists(QStringLiteral("report.sh")), "DEFRAGR_FAKES is not set");
    }

    void init() { useEngine("report.sh"); }

    // While it runs, the report says how many it has found and which was
    // last, in the order the engine sent them.
    void foundWhileRunning()
    {
        // Declared first, so they outlive the report and its connection.
        quint64 count = 0;
        QByteArray latest;
        Report report(fake("pass-through.sh"));
        connect(&report, &Report::found, this, [&](quint64 found, const QByteArray &path) {
            count = found;
            latest = path;
        });
        run(report, "/mnt/fake");
        QCOMPARE(count, quint64(3));
        QCOMPARE(latest, QByteArray("games/tie.pak"));
    }

    // S3: the worst file first; of two in as many pieces, the bigger.
    void worstFirst()
    {
        Report report(fake("pass-through.sh"));
        const Load load = run(report, "/mnt/fake");
        QCOMPARE(load.failures, QList<Message>());
        QCOMPARE(load.loaded, 1);
        QCOMPARE(report.rowCount(), 3);
        const auto cell = [&](int row, int column) {
            return report.index(row, column).data().toString();
        };
        QCOMPARE(cell(0, Report::ColumnFile), QStringLiteral("/mnt/fake/games/big.iso"));
        QCOMPARE(cell(1, Report::ColumnFile), QStringLiteral("/mnt/fake/games/tie.pak"));
        QCOMPARE(cell(2, Report::ColumnFile), QStringLiteral("/mnt/fake/music/song.flac"));
        QCOMPARE(cell(0, Report::ColumnPieces), QLocale().toString(40));
        // 1000 blocks of HELLO's 4096 bytes.
        QCOMPARE(cell(0, Report::ColumnSize), QLocale().formattedDataSize(4096000));
    }

    // S3: the figures come from REPORT_SUMMARY.
    void figuresFromTheSummary()
    {
        Report report(fake("pass-through.sh"));
        run(report, "/mnt/fake");
        ReportSummary expected = summary(200, 3, 250, 1);
        expected.fileBlocks = 5000;
        QCOMPARE(report.summary(), expected);
        report.run("/mnt/fake");
        // A new run empties the old report at once.
        QCOMPARE(report.rowCount(), 0);
        QCOMPARE(report.summary(), ReportSummary());
        QVERIFY(QTest::qWaitFor([&] { return !report.isRunning(); }, 10000));
    }

    // A drive mounted at / gives paths with one slash, not two.
    void rootDrive()
    {
        Report report(fake("pass-through.sh"));
        run(report, "/");
        QCOMPARE(report.index(0, Report::ColumnFile).data().toString(),
                 QStringLiteral("/games/big.iso"));
    }

    // The mount point reaches the engine as its argument: the fake
    // answers only for /mnt/fake and /.
    void wrongDriveIsNotReported()
    {
        Report report(fake("pass-through.sh"));
        const Load load = run(report, "/mnt/other");
        QCOMPARE(load.loaded, 0);
        QCOMPARE(load.failures.size(), 1);
    }

    // A broken run shows a message and no figures, never a guess.
    void brokenEngineShowsNothing()
    {
        Report report(fake("pass-through.sh"));
        run(report, "/mnt/fake");
        useEngine("crash.sh");
        const Load load = run(report, "/mnt/fake");
        QCOMPARE(load.loaded, 0);
        QCOMPARE(load.failures.size(), 1);
        QVERIFY(!load.failures.first().what.isEmpty());
        QCOMPARE(report.rowCount(), 0);
        QCOMPARE(report.summary(), ReportSummary());
    }

    // A report needs admin rights, so it goes through the launcher.
    void runsThroughTheLauncher()
    {
        Report report(fake("exit-126.sh"));
        const Load load = run(report, "/mnt/fake");
        QCOMPARE(load.failures.size(), 1);
        QCOMPARE(load.failures.first(), messages::forRun({RunEnd::AdminRefused, {}, {}}));
    }

    void wording_data()
    {
        QTest::addColumn<ReportSummary>("summary");
        QTest::addColumn<QStringList>("lines");

        QTest::newRow("from the fake")
            << summary(200, 3, 250, 1)
            << QStringList{QStringLiteral("3 of 200 files checked are fragmented (1.5%)."),
                           QStringLiteral("On average a file is in 1.25 pieces."),
                           QStringLiteral("1 file or folder could not be read, so it is not "
                                          "counted.")};
        QTest::newRow("one of one")
            << summary(1, 1, 2)
            << QStringList{QStringLiteral("1 of 1 file checked is fragmented (100%)."),
                           QStringLiteral("On average a file is in 2 pieces.")};
        QTest::newRow("none fragmented")
            << summary(10, 0, 10, 2)
            << QStringList{QStringLiteral("0 of 10 files checked are fragmented (0%)."),
                           QStringLiteral("On average a file is in 1 piece."),
                           QStringLiteral("2 files or folders could not be read, so they are "
                                          "not counted.")};
        // A few among millions is not 0%.
        QTest::newRow("tiny share")
            << summary(5000000, 2, 5000002)
            << QStringList{QStringLiteral("2 of 5,000,000 files checked are fragmented (under "
                                          "0.1%)."),
                           QStringLiteral("On average a file is in 1 piece.")};
        QTest::newRow("empty drive")
            << summary(0, 0, 0)
            << QStringList{QStringLiteral("DeFragR found no files on this drive.")};
    }

    void wording()
    {
        QFETCH(ReportSummary, summary);
        QFETCH(QStringList, lines);
        QCOMPARE(summaryLines(summary), lines);
    }
};

QTEST_GUILESS_MAIN(TestReport)
#include "tst_report.moc"
