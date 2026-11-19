// RunState: one defrag of a folder (docs/specs/DFR-0018-window-folder-defrag.md
// INV-3 to INV-5), run against fakes/window.sh through stand-in pkexecs.
// The fake logs each command line it is started with, so the test sees
// which engine runs were made and with what arguments.
#include "FakeHold.h"
#include "ui/Languages.h"
#include "model/RunState.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace defragr::engine;
using namespace defragr::model;
// The engine has a record called RunState too, so the model's gets a
// name of its own here.
using Run = defragr::model::RunState;

namespace {

const QByteArray kMount = QByteArrayLiteral("/mnt/<img src=x>");

// The fake's map (fakes/window.sh), one block per cell.
QList<CellState> fakeCells(bool afterDefrag)
{
    QList<CellState> cells;
    const auto put = [&](int count, CellState state) { cells += QList<CellState>(count, state); };
    using enum CellState;
    put(100, Whole);
    put(20, afterDefrag ? Free : Fragmented);
    put(80, Filesystem);
    put(100, Free);
    put(20, afterDefrag ? Free : Fragmented);
    put(80, Free);
    put(40, afterDefrag ? Whole : Free);
    put(560, Free);
    return cells;
}

}  // namespace

class TestRunState : public QObject {
    Q_OBJECT

    // English's plural forms, as the window has them.
    defragr::ui::Languages m_english{QLocale(QLocale::English)};
    QDir m_fakes;
    QTemporaryDir m_scratch;
    QString m_log;

    QString fake(const char *name) const { return m_fakes.filePath(QLatin1String(name)); }
    QString launcher(const char *folder) const
    {
        return QDir(fake(folder)).filePath(QStringLiteral("pkexec"));
    }

    // The command lines the fake has been started with.
    QStringList commandLines() const
    {
        QFile log(m_log);
        if (!log.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(log.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

    static bool runToEnd(Run &run, const QByteArray &folder)
    {
        bool ended = false;
        QObject listening;
        connect(&run, &Run::ended, &listening, [&] { ended = true; });
        run.start(kMount, folder);
        return QTest::qWaitFor([&] { return ended; }, 15000);
    }

private slots:
    void initTestCase()
    {
        m_fakes.setPath(qEnvironmentVariable("DEFRAGR_FAKES"));
        QVERIFY2(m_fakes.exists(QStringLiteral("window.sh")), "DEFRAGR_FAKES is not set");
        QVERIFY(m_scratch.isValid());
        m_log = m_scratch.filePath(QStringLiteral("log"));
        qputenv("DEFRAGR_TEST_LOG", QFile::encodeName(m_log));
        qputenv("DEFRAGR_ENGINE", QFile::encodeName(fake("window.sh")));
    }

    void init()
    {
        QFile::remove(m_log);
        qunsetenv("DEFRAGR_TEST_DEFRAG");
    }

    // INV-4.
    void folderOnDrive_data()
    {
        QTest::addColumn<QByteArray>("mount");
        QTest::addColumn<QString>("chosen");
        QTest::addColumn<bool>("onDrive");
        QTest::addColumn<QByteArray>("folder");

        const QByteArray games("/mnt/Games");
        QTest::newRow("the drive itself") << games << "/mnt/Games" << true << QByteArray();
        QTest::newRow("with a slash") << games << "/mnt/Games/" << true << QByteArray();
        QTest::newRow("a folder") << games << "/mnt/Games/Old" << true << QByteArray("Old");
        QTest::newRow("deep, untidy") << games << "/mnt/Games//Old/./ROMs/" << true
                                      << QByteArray("Old/ROMs");
        QTest::newRow("a name that starts the same") << games << "/mnt/GamesOld" << false
                                                     << QByteArray();
        QTest::newRow("out through ..") << games << "/mnt/Games/../Emulators" << false
                                        << QByteArray();
        QTest::newRow("back in through ..") << games << "/mnt/Games/Old/../New" << true
                                            << QByteArray("New");
        QTest::newRow("above") << games << "/mnt" << false << QByteArray();
        QTest::newRow("not absolute") << games << "Old" << false << QByteArray();
        QTest::newRow("the root drive") << QByteArray("/") << "/home/me" << true
                                        << QByteArray("home/me");
        QTest::newRow("the root itself") << QByteArray("/") << "/" << true << QByteArray();
        QTest::newRow("no drive") << QByteArray() << "/mnt/Games" << false << QByteArray();
    }

    void folderOnDrive()
    {
        QFETCH(QByteArray, mount);
        QFETCH(QString, chosen);
        QFETCH(bool, onDrive);
        QFETCH(QByteArray, folder);
        const std::optional<QByteArray> got = defragr::model::folderOnDrive(mount, chosen);
        QCOMPARE(got.has_value(), onDrive);
        if (onDrive)
            QCOMPARE(*got, folder);
    }

    // INV-3 and INV-5: with no map, one run that sends the map and then
    // defrags; the counts are the stream's, and the map is the one sent,
    // with the moves made.
    void readsTheMapThenDefrags()
    {
        Run run(launcher("pkexec-pass"));
        QList<Run::Stage> stages;
        int moves = 0;
        QList<CellState> beforeTheMoves;
        connect(&run, &Run::stageChanged, this, [&] {
            stages.append(run.stage());
            if (run.stage() == Run::Stage::Defragging)
                beforeTheMoves = run.map().cells(1000);
        });
        connect(&run, &Run::blocksMoved, this, [&] { ++moves; });
        QVERIFY(!run.hasMapOf(kMount));
        QVERIFY(runToEnd(run, "games"));

        QCOMPARE(stages, (QList{Run::Stage::ReadingMap, Run::Stage::Defragging,
                                Run::Stage::Done}));
        QCOMPARE(commandLines(),
                 QStringList{QStringLiteral("defrag /mnt/<img src=x> games map")});
        QCOMPARE(beforeTheMoves, fakeCells(false));
        QCOMPARE(moves, 1);
        QCOMPARE(run.counts(), (Run::Counts{1, 1, 1, 40}));
        // DFR-0002 INV-26: the last word on how far it got.
        QCOMPARE(run.work(), std::optional(DefragProgress{3, 3, 160, 160}));
        QCOMPARE(run.latestPath(), QByteArray("games/<b>busy</b>.bin"));
        QCOMPARE(run.skipped().size(), 1);
        QCOMPARE(run.skipped().first().stage, quint16(StageChanged));
        QCOMPARE(run.problem(), Message{});
        QVERIFY(run.hasMapOf(kMount));
        QCOMPARE(run.map().cells(1000), fakeCells(true));
    }

    // INV-3: a map it holds is used, from an earlier run or handed in,
    // and the folder goes to the engine escaped as a mount point is.
    void aHeldMapIsNotReadAgain()
    {
        Run run(launcher("pkexec-pass"));
        QVERIFY(runToEnd(run, "games"));
        QFile::remove(m_log);
        QVERIFY(runToEnd(run, QByteArray("back\\slash")));
        QCOMPARE(commandLines(),
                 QStringList{QStringLiteral("defrag /mnt/<img src=x> back\\134slash")});
        QCOMPARE(run.stage(), Run::Stage::Done);

        Run other(launcher("pkexec-pass"));
        DriveMap map;
        map.add({MapFragmentedFile, {{0, 1000}}});
        other.setMap(kMount, map);
        QVERIFY(other.hasMapOf(kMount));
        QVERIFY(!other.hasMapOf("/mnt/other"));
        QFile::remove(m_log);
        QVERIFY(runToEnd(other, ""));
        QCOMPARE(commandLines(), QStringList{QStringLiteral("defrag /mnt/<img src=x> ")});
        QCOMPARE(other.map().cells(1000).mid(400, 40), QList<CellState>(40, CellState::Whole));
    }

    // INV-3: refused admin rights, the run fails with no map and nothing
    // started.
    void aRefusedRunHoldsNoMap()
    {
        Run run(launcher("pkexec-refuse"));
        QVERIFY(runToEnd(run, "games"));
        QCOMPARE(run.stage(), Run::Stage::Failed);
        RunOutcome refused;
        refused.end = RunEnd::AdminRefused;
        refused.enginePath = fake("window.sh");
        QCOMPARE(run.problem(), messages::forRun(refused));
        QCOMPARE(commandLines(), QStringList{});
        QVERIFY(!run.hasMapOf(kMount));
        QVERIFY(run.map().isEmpty());
    }

    // DFR-0106: a file that ends in two or more pieces averaging 16 MiB,
    // by its FILE_DEFRAGGED and the run's HELLO's block size, shows as
    // large pieces where it landed.
    void aFileInLargePiecesShowsSo()
    {
        qputenv("DEFRAGR_TEST_DEFRAG", "large");
        Run run(launcher("pkexec-pass"));
        QVERIFY(runToEnd(run, "games"));
        QCOMPARE(run.stage(), Run::Stage::Done);
        QCOMPARE(run.map().cells(1000).mid(400, 40),
                 QList<CellState>(40, CellState::LargePieces));
    }

    // INV-5: a run the engine stops still finished its stream, so the
    // counts and the map so far stand, and the result's words are given.
    void aStoppedRunKeepsWhatItDid()
    {
        qputenv("DEFRAGR_TEST_DEFRAG", "mismatch");
        Run run(launcher("pkexec-pass"));
        QVERIFY(runToEnd(run, "games"));
        QCOMPARE(run.stage(), Run::Stage::Failed);
        RunOutcome mismatch;
        mismatch.end = RunEnd::Finished;
        mismatch.finished.result = ResultChecksumMismatch;
        QCOMPARE(run.problem().what, messages::forRun(mismatch).what);
        QCOMPARE(run.counts(), (Run::Counts{1, 0, 1, 50}));
        QVERIFY(run.hasMapOf(kMount));
        // The file that went wrong was moved, and is not known whole.
        QCOMPARE(run.map().cells(1000).mid(500, 10),
                 QList<CellState>(10, CellState::Fragmented));
        QCOMPARE(run.map().cells(1000).mid(400, 40), QList<CellState>(40, CellState::Whole));
    }

    // INV-5: after a broken stream the map may not follow the drive, so
    // it is dropped, and the next run reads a new one.
    void aBrokenRunDropsTheMap()
    {
        qputenv("DEFRAGR_TEST_DEFRAG", "broken");
        Run run(launcher("pkexec-pass"));
        QVERIFY(runToEnd(run, "games"));
        QCOMPARE(run.stage(), Run::Stage::Failed);
        QVERIFY(!run.problem().what.isEmpty());
        QVERIFY(!run.hasMapOf(kMount));
        QVERIFY(run.map().isEmpty());

        qunsetenv("DEFRAGR_TEST_DEFRAG");
        QFile::remove(m_log);
        QVERIFY(runToEnd(run, "games"));
        QCOMPARE(commandLines(),
                 QStringList{QStringLiteral("defrag /mnt/<img src=x> games map")});
        QCOMPARE(run.stage(), Run::Stage::Done);
        QCOMPARE(run.counts(), (Run::Counts{1, 1, 1, 40}));
    }

    // DFR-0099: how far a run has got, by blocks, and never past whole.
    void workAsAPercentageAndASentence()
    {
        QCOMPARE(percentDone({0, 3, 0, 160}), 0);
        QCOMPARE(percentDone({1, 3, 40, 160}), 25);
        QCOMPARE(percentDone({3, 3, 160, 160}), 100);
        // A folder that changed since it was counted, and one with no work.
        QCOMPARE(percentDone({4, 3, 900, 160}), 100);
        QCOMPARE(percentDone({0, 0, 0, 0}), 100);
        // By files, where the blocks say nothing.
        QCOMPARE(percentDone({1, 2, 0, 0}), 50);

        QCOMPARE(workLine({1, 3, 40, 160}),
                 QStringLiteral("Finished with 1 of 3 files in pieces (25%)."));
        QCOMPARE(workLine({4, 3, 900, 160}),
                 QStringLiteral("Finished with 3 of 3 files in pieces (100%)."));
        QCOMPARE(workLine({0, 1, 0, 2}),
                 QStringLiteral("Finished with 0 of 1 file in pieces (0%)."));
        QCOMPARE(workLine({0, 0, 0, 0}), QStringLiteral("No file in this folder is in pieces."));
    }

    // DFR-0030: the time left is the time so far, scaled by the work
    // left, once there is enough of both to go by.
    void timeLeftFromThePaceSoFar()
    {
        // A quarter done in 30 s leaves three times that.
        QCOMPARE(secondsLeft({1, 3, 40, 160}, 30), 90);
        // By files, where the blocks say nothing.
        QCOMPARE(secondsLeft({1, 2, 0, 0}, 30), 30);
        // Too early: under ten seconds of work, or nothing done yet.
        QCOMPARE(secondsLeft({1, 3, 40, 160}, 9), -1);
        QCOMPARE(secondsLeft({0, 3, 0, 160}, 600), -1);
        // No work to do, and a folder that changed since it was counted.
        QCOMPARE(secondsLeft({0, 0, 0, 0}, 600), -1);
        QCOMPARE(secondsLeft({4, 3, 900, 160}, 600), 0);

        QCOMPARE(timeLeftLine(0), QStringLiteral("Less than a minute left."));
        QCOMPARE(timeLeftLine(59), QStringLiteral("Less than a minute left."));
        QCOMPARE(timeLeftLine(90), QStringLiteral("About 2 min left."));
        QCOMPARE(timeLeftLine(3540), QStringLiteral("About 59 min left."));
        // Rounding to the minute carries into the hour.
        QCOMPARE(timeLeftLine(3590), QStringLiteral("About 1 h 0 min left."));
        QCOMPARE(timeLeftLine(4000), QStringLiteral("About 1 h 7 min left."));
    }

    // A new run starts with no word on its work.
    void aNewRunForgetsTheLastOnesWork()
    {
        Run run(launcher("pkexec-pass"));
        QVERIFY(runToEnd(run, "games"));
        QVERIFY(run.work());
        FakeHold hold;
        QVERIFY(hold.isValid());
        run.start(kMount, "games");
        QVERIFY(!run.work());
        QVERIFY(hold.release());
    }

    // INV-5: a run that ends before its map is whole shows no part of
    // it, and says why it ended.
    void aRunThatEndsInItsMapHoldsNone()
    {
        qputenv("DEFRAGR_TEST_DEFRAG", "no-map");
        Run run(launcher("pkexec-pass"));
        quint64 names = 0;
        connect(&run, &Run::mapProgressed, this, [&](quint64 read, quint64) { names = read; });
        QVERIFY(runToEnd(run, "games"));
        QCOMPARE(names, quint64(300));
        QCOMPARE(run.stage(), Run::Stage::Failed);
        RunOutcome error;
        error.end = RunEnd::Finished;
        error.finished = {ResultDriveError, 5};
        QCOMPARE(run.problem(), messages::forRun(error));
        QVERIFY(!run.hasMapOf(kMount));
        QVERIFY(run.map().isEmpty());
        QCOMPARE(run.counts(), Run::Counts{});
    }
};

QTEST_GUILESS_MAIN(TestRunState)
#include "tst_runstate.moc"
