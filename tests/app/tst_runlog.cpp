// The developers' log (ROADMAP DFR-0111): what it writes, which of the
// engine's records it notes, and that it is silent until started.
#include "engine/RunLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using namespace defragr::engine;

namespace {

QString contents(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

}  // namespace

class TestRunLog : public QObject {
    Q_OBJECT

private slots:
    // A build that never starts the log notes nothing, so a released
    // build prints none of this on its terminal either.
    void silentUntilStarted()
    {
        QVERIFY(!runLog().isInfoEnabled());
        QVERIFY(runLog().isWarningEnabled());
    }

    void writesEachLineWithItsTime()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("state/defragr.log"));
        // The folder is made when it is not there.
        QVERIFY(startRunLog(folder.filePath(QStringLiteral("state"))));
        QVERIFY(runLog().isInfoEnabled());
        qCInfo(runLog) << "first line";
        qCWarning(runLog) << "second line";
        stopRunLog();
        qCWarning(runLog) << "after the log stopped";

        const QStringList lines = contents(path).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 2);
        // A time to the millisecond, the level, the category, the words.
        const QRegularExpression shape(QStringLiteral(
            R"(^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3} ([IW]) defragr\.run: (.*)$)"));
        const auto first = shape.match(lines.at(0));
        QVERIFY2(first.hasMatch(), qPrintable(lines.at(0)));
        QCOMPARE(first.captured(1), QStringLiteral("I"));
        QCOMPARE(first.captured(2), QStringLiteral("first line"));
        QCOMPARE(shape.match(lines.at(1)).captured(1), QStringLiteral("W"));
        QVERIFY(!runLog().isInfoEnabled());
    }

    // The file does not grow without end: a big one becomes the one
    // older file kept, and a new one starts.
    void aBigLogIsSetAside()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("defragr.log"));
        {
            QFile big(path);
            QVERIFY(big.open(QIODevice::WriteOnly));
            QVERIFY(big.resize(kRunLogBytes + 1));
        }
        QFile stale(path + QStringLiteral(".1"));
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.write("older still");
        stale.close();

        QVERIFY(startRunLog(folder.path()));
        qCInfo(runLog) << "a new file";
        stopRunLog();
        QCOMPARE(QFileInfo(path + QStringLiteral(".1")).size(), kRunLogBytes + 1);
        QVERIFY(contents(path).contains(QStringLiteral("a new file")));

        // A small one is added to.
        QVERIFY(startRunLog(folder.path()));
        qCInfo(runLog) << "the same file";
        stopRunLog();
        QVERIFY(contents(path).contains(QStringLiteral("a new file")));
        QVERIFY(contents(path).contains(QStringLiteral("the same file")));
    }

    void aFolderThatCannotBeMadeStartsNothing()
    {
        QVERIFY(!startRunLog(QStringLiteral("/proc/no-such-folder/defragr")));
        QVERIFY(!runLog().isInfoEnabled());
    }

    // Progress is noted when its whole percentage changes, so a long
    // run's log stays short; a move of blocks is never noted.
    void notesTheRecordsThatTellARunsStory()
    {
        RecordNotes notes;
        QCOMPARE(notes.note(Hello{1, 7, 3, 4096}),
                 QStringLiteral("HELLO 1.7 command 3 block size 4096"));
        QCOMPARE(notes.note(ReportProgress{0, 300}),
                 QStringLiteral("REPORT_PROGRESS names 0 of about 300"));
        QCOMPARE(notes.note(ReportProgress{1, 300}), QString());
        QCOMPARE(notes.note(ReportProgress{2, 300}), QString());
        QCOMPARE(notes.note(ReportProgress{3, 300}),
                 QStringLiteral("REPORT_PROGRESS names 3 of about 300"));
        // With no estimate there is no percentage: each is noted.
        QCOMPARE(notes.note(ReportProgress{5, 0}),
                 QStringLiteral("REPORT_PROGRESS names 5 of about 0"));
        QCOMPARE(notes.note(MapDone{}), QStringLiteral("MAP_DONE"));
        QCOMPARE(notes.note(DefragProgress{0, 3, 0, 200}),
                 QStringLiteral("DEFRAG_PROGRESS files 0 of 3, blocks 0 of 200"));
        QCOMPARE(notes.note(DefragProgress{1, 3, 1, 200}), QString());
        QCOMPARE(notes.note(DefragProgress{2, 3, 100, 200}),
                 QStringLiteral("DEFRAG_PROGRESS files 2 of 3, blocks 100 of 200"));
        QCOMPARE(notes.note(BlocksMoved{}), QString());
        QCOMPARE(notes.note(MapRuns{}), QString());
        FileSkipped skipped;
        skipped.errnum = 16;
        skipped.stage = 4;
        skipped.path = "games/busy.bin";
        QCOMPARE(notes.note(skipped),
                 QStringLiteral("FILE_SKIPPED stage 4 errno 16 games/busy.bin"));
        QCOMPARE(notes.note(Finished{0, 0}), QStringLiteral("FINISH result 0 errno 0"));
    }

    // $XDG_STATE_HOME when it is set, as the desktop's rules say.
    void theFolderFollowsTheDesktopsRule()
    {
        qputenv("XDG_STATE_HOME", "/somewhere/state");
        QCOMPARE(runLogFolder(), QStringLiteral("/somewhere/state/defragr"));
        qunsetenv("XDG_STATE_HOME");
        QCOMPARE(runLogFolder(), QDir::homePath() + QStringLiteral("/.local/state/defragr"));
    }
};

QTEST_GUILESS_MAIN(TestRunLog)
#include "tst_runlog.moc"
