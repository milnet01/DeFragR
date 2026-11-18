// The fake engine plays a recorded run back through EngineProcess
// unchanged, at once or paced, and the recording still decodes
// (docs/design.md § How it is tested, Recorded runs). DEFRAGR_ENGINE is
// the fake engine, DEFRAGR_RECORDING its recording, and DEFRAGR_FAKES
// holds the launcher that stands in for pkexec.
#include "engine/EngineProcess.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTest>

#include <algorithm>

using namespace defragr::engine;

namespace {

struct Run {
    RunOutcome outcome;
    QList<Record> records;
    int batches = 0;  // recordsReceived signals
    bool ended = false;
};

Run run(Command command, const QStringList &arguments = {})
{
    EngineProcess engine(QDir(qEnvironmentVariable("DEFRAGR_FAKES"))
                             .filePath(QStringLiteral("pass-through.sh")));
    Run result;
    QObject::connect(&engine, &EngineProcess::recordsReceived,
                     [&](const QList<Record> &records) {
                         result.records += records;
                         ++result.batches;
                     });
    QObject::connect(&engine, &EngineProcess::ended, [&](const RunOutcome &outcome) {
        result.outcome = outcome;
        result.ended = true;
    });
    engine.start(command, arguments);
    if (!QTest::qWaitFor([&] { return result.ended; }, 10000))
        qWarning("the run did not end");
    return result;
}

template <typename Type>
bool has(const QList<Record> &records)
{
    return std::ranges::any_of(records, [](const Record &record) {
        return std::holds_alternative<Type>(record);
    });
}

}  // namespace

class TestReplay : public QObject {
    Q_OBJECT

    QList<Record> m_recorded;  // the recording, decoded straight from its file

private slots:
    void initTestCase()
    {
        QFile file(QDir(qEnvironmentVariable("DEFRAGR_RECORDING"))
                       .filePath(QStringLiteral("report.bin")));
        QVERIFY2(file.open(QIODevice::ReadOnly), "DEFRAGR_RECORDING has no report.bin");
        RecordDecoder decoder;
        decoder.feed(file.readAll());
        decoder.finish();
        QVERIFY2(!decoder.error(), "the recording no longer decodes: record it again");
        m_recorded = decoder.takeRecords();
    }

    void init() { qunsetenv("DEFRAGR_REPLAY_SECONDS"); }

    // The recording is a whole report that drew a map: a test built on it
    // has every kind of record to work with.
    void recordingIsAWholeReport()
    {
        QVERIFY(!m_recorded.isEmpty());
        const auto *hello = std::get_if<Hello>(&m_recorded.first());
        QVERIFY(hello);
        QCOMPARE(hello->command, quint32(CommandReport));
        const auto *finished = std::get_if<Finished>(&m_recorded.last());
        QVERIFY(finished);
        QCOMPARE(finished->result, qint32(ResultOk));
        QVERIFY(has<FragmentedFile>(m_recorded));
        QVERIFY(has<MapRuns>(m_recorded));
        QVERIFY(has<ReportSummary>(m_recorded));
    }

    // At once: the window gets every record, as recorded, and a finished run.
    void replaysAtOnce()
    {
        const Run report = run(CommandReport, {QStringLiteral("/mnt/img")});
        QCOMPARE(report.outcome.end, RunEnd::Finished);
        QCOMPARE(report.outcome.finished.result, qint32(ResultOk));
        QCOMPARE(report.records, m_recorded);
    }

    // Paced: the same records, arriving over the time asked for rather
    // than in one go.
    void replaysPaced()
    {
        qputenv("DEFRAGR_REPLAY_SECONDS", "1");
        QElapsedTimer timer;
        timer.start();
        const Run report = run(CommandReport, {QStringLiteral("/mnt/img")});
        QVERIFY2(timer.elapsed() >= 900, qPrintable(QString::number(timer.elapsed())));
        QCOMPARE(report.outcome.end, RunEnd::Finished);
        QCOMPARE(report.records, m_recorded);
        QVERIFY2(report.batches > m_recorded.size() / 2, qPrintable(QString::number(report.batches)));
    }

    // A command with no recording gets no stream, so the window reads the
    // fake as a broken engine rather than a finished run.
    void noRecordingIsBroken()
    {
        const Run probe = run(CommandProbe);
        QVERIFY(probe.ended);  // an outcome left at its default reads as Broken
        QCOMPARE(probe.outcome.end, RunEnd::Broken);
        QVERIFY(probe.records.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestReplay)
#include "tst_replay.moc"
