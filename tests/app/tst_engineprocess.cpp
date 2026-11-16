// EngineProcess tells each way a run can end from the others
// (docs/specs/DFR-0002-engine-protocol.md § 6; docs/design.md § Errors).
// The real engine comes from DEFRAGR_ENGINE; the failures come from the
// fake engines and launchers in DEFRAGR_FAKES.
#include "engine/EngineProcess.h"

#include <QDir>
#include <QFile>
#include <QTest>

using namespace defragr::engine;

namespace {

struct Run {
    RunOutcome outcome;
    QList<Record> records;
    int ends = 0;
    int endsDuringStart = 0;  // ended() emitted before start() returned
};

Run run(EngineProcess &engine, Command command, const QStringList &arguments = {})
{
    Run result;
    QObject::connect(&engine, &EngineProcess::recordsReceived,
                     [&](const QList<Record> &records) { result.records += records; });
    QObject::connect(&engine, &EngineProcess::ended, [&](const RunOutcome &outcome) {
        result.outcome = outcome;
        ++result.ends;
    });
    engine.start(command, arguments);
    result.endsDuringStart = result.ends;
    if (!QTest::qWaitFor([&] { return result.ends > 0; }, 10000))
        qWarning("the run did not end");
    QTest::qWait(50);  // a second ended() would land here
    return result;
}

std::optional<Hello> hello(const QList<Record> &records)
{
    for (const Record &record : records) {
        if (const auto *found = std::get_if<Hello>(&record))
            return *found;
    }
    return std::nullopt;
}

}  // namespace

class TestEngineProcess : public QObject {
    Q_OBJECT

    QByteArray m_engine;
    QDir m_fakes;

    QString fake(const char *name) const { return m_fakes.filePath(QLatin1String(name)); }
    void useEngine(const QString &path) { qputenv("DEFRAGR_ENGINE", QFile::encodeName(path)); }

private slots:
    void initTestCase()
    {
        m_engine = qgetenv("DEFRAGR_ENGINE");
        QVERIFY2(!m_engine.isEmpty(), "DEFRAGR_ENGINE is not set");
        m_fakes.setPath(qEnvironmentVariable("DEFRAGR_FAKES"));
        QVERIFY2(m_fakes.exists(QStringLiteral("crash.sh")), "DEFRAGR_FAKES is not set");
    }

    void init() { qputenv("DEFRAGR_ENGINE", m_engine); }

    // The probe runs without a launcher and finishes.
    void probeFinishes()
    {
        EngineProcess engine(fake("exit-126.sh"));
        const Run probe = run(engine, CommandProbe);
        QCOMPARE(probe.ends, 1);
        QCOMPARE(probe.outcome.end, RunEnd::Finished);
        QCOMPARE(probe.outcome.finished.result, qint32(ResultOk));
        QCOMPARE(hello(probe.records).value_or(Hello{}).command, quint32(CommandProbe));
    }

    // A command other than probe goes through the launcher, which is
    // handed the engine and its whole command line.
    void launcherRunsTheEngine()
    {
        EngineProcess engine(fake("pass-through.sh"));
        const Run report = run(engine, CommandReport, {QStringLiteral("/")});
        QCOMPARE(report.outcome.end, RunEnd::Finished);
        QCOMPARE(hello(report.records).value_or(Hello{}).command, quint32(CommandReport));
    }

    // § 6: pkexec exiting 126 or 127 with no stream is refused admin
    // rights, not a broken engine.
    void launcherRefusal()
    {
        EngineProcess engine(fake("exit-126.sh"));
        const Run report = run(engine, CommandReport, {QStringLiteral("/")});
        QCOMPARE(report.ends, 1);
        QCOMPARE(report.outcome.end, RunEnd::AdminRefused);
    }

    void launcherMissing()
    {
        EngineProcess engine(QStringLiteral("/nonexistent/pkexec"));
        const Run report = run(engine, CommandReport, {QStringLiteral("/")});
        QCOMPARE(report.ends, 1);
        QCOMPARE(report.outcome.end, RunEnd::AdminRefused);
    }

    // The same exit from the engine itself is a broken engine: the
    // refusal reading belongs to the launcher alone.
    void engineExiting126IsBroken()
    {
        useEngine(fake("exit-126.sh"));
        EngineProcess engine;
        QCOMPARE(run(engine, CommandProbe).outcome.end, RunEnd::Broken);
    }

    void crashIsBroken()
    {
        useEngine(fake("crash.sh"));
        EngineProcess engine;
        QCOMPARE(run(engine, CommandProbe).outcome.end, RunEnd::Broken);
    }

    void wrongMajor()
    {
        useEngine(fake("wrong-major.sh"));
        EngineProcess engine;
        QCOMPARE(run(engine, CommandProbe).outcome.end, RunEnd::VersionMismatch);
    }

    // ADR-0002: a missing engine is said plainly, and named. ended()
    // still comes after start() returns, as it does for every other end.
    void missingEngine()
    {
        const QString path = QStringLiteral("/nonexistent/defragr-engine");
        useEngine(path);
        EngineProcess engine;
        const Run probe = run(engine, CommandProbe);
        QCOMPARE(probe.endsDuringStart, 0);
        QCOMPARE(probe.ends, 1);
        QCOMPARE(probe.outcome.end, RunEnd::EngineMissing);
        QCOMPARE(probe.outcome.enginePath, path);
    }
};

QTEST_GUILESS_MAIN(TestEngineProcess)
#include "tst_engineprocess.moc"
