// EngineProcess tells each way a run can end from the others
// (docs/specs/DFR-0002-engine-protocol.md § 6; docs/design.md § Errors).
// The real engine comes from DEFRAGR_ENGINE; the failures come from the
// fake engines and launchers in DEFRAGR_FAKES.
#include "FakeHold.h"
#include "engine/EngineProcess.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <memory>

#include <unistd.h>

using namespace defragr::engine;

namespace {

struct Run {
    RunOutcome outcome;
    QList<Record> records;
    int ends = 0;
    int endsDuringStart = 0;  // ended() emitted before start() returned
};

// Once per engine: its connections outlive the Run they write to. An
// outcome left at its default reads as Broken, so a test of a broken end
// also checks that ended() came. The run is over once the engine has
// exited and no end is queued; a second ended() would have come by then,
// or be among the events still posted.
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
    if (!QTest::qWaitFor([&] { return result.ends > 0 && !engine.isRunning(); }, 10000))
        qWarning("the run did not end");
    QCoreApplication::sendPostedEvents();
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
        // The engine got its folder: without one it would answer USAGE.
        if (geteuid() == 0)
            QSKIP("runs as root, so the admin-rights check passes");
        QCOMPARE(report.outcome.finished.result, qint32(ResultNotRoot));
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

    // DFR-0049: a launcher that cannot start asked nobody for a
    // password, so it is not a refusal.
    void launcherMissing()
    {
        EngineProcess engine(QStringLiteral("/nonexistent/pkexec"));
        const Run report = run(engine, CommandReport, {QStringLiteral("/")});
        QCOMPARE(report.ends, 1);
        QCOMPARE(report.outcome.end, RunEnd::LauncherMissing);
    }

    // The same exit from the engine itself is a broken engine: the
    // refusal reading belongs to the launcher alone.
    void engineExiting126IsBroken()
    {
        useEngine(fake("exit-126.sh"));
        EngineProcess engine;
        const Run probe = run(engine, CommandProbe);
        QCOMPARE(probe.ends, 1);
        QCOMPARE(probe.outcome.end, RunEnd::Broken);
    }

    void crashIsBroken()
    {
        useEngine(fake("crash.sh"));
        EngineProcess engine;
        const Run probe = run(engine, CommandProbe);
        QCOMPARE(probe.ends, 1);
        QCOMPARE(probe.outcome.end, RunEnd::Broken);
    }

    void wrongMajor()
    {
        useEngine(fake("wrong-major.sh"));
        EngineProcess engine;
        QCOMPARE(run(engine, CommandProbe).outcome.end, RunEnd::VersionMismatch);
    }

    // DFR-0051: an answer to another command is a broken engine, not an
    // empty report. The same stream answers a probe properly.
    void wrongCommandIsBroken()
    {
        useEngine(fake("wrong-command.sh"));
        EngineProcess report(fake("pass-through.sh"));
        const Run reported = run(report, CommandReport, {QStringLiteral("/")});
        QCOMPARE(reported.ends, 1);
        QCOMPARE(reported.outcome.end, RunEnd::Broken);
        EngineProcess probe;
        QCOMPARE(run(probe, CommandProbe).outcome.end, RunEnd::Finished);
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

    // DFR-0077: a run is going until its ended() has come, so a second
    // start() before then is ignored and the run ends once.
    void startWhileEndingIsIgnored()
    {
        useEngine(QStringLiteral("/nonexistent/defragr-engine"));
        int ends = 0;
        EngineProcess engine;
        connect(&engine, &EngineProcess::ended, this, [&] { ++ends; });
        engine.start(CommandProbe);
        QVERIFY(engine.isRunning());
        engine.start(CommandProbe);
        QTRY_VERIFY(ends > 0 && !engine.isRunning());
        QCoreApplication::sendPostedEvents();  // a second ended() would land here
        QCOMPARE(ends, 1);
    }

    // DFR-0077: an engine program that is there but will not start, such
    // as one that is not a program at all, is named as such, not as
    // missing.
    void unstartableEngineIsNotMissing()
    {
        QTemporaryDir dir(QDir::current().filePath(QStringLiteral("unstartable-XXXXXX")));
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("defragr-engine")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a program\n");
        file.close();
        QVERIFY(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));
        useEngine(file.fileName());
        EngineProcess engine;
        const Run probe = run(engine, CommandProbe);
        QCOMPARE(probe.ends, 1);
        QCOMPARE(probe.outcome.end, RunEnd::EngineUnstartable);
        QCOMPARE(probe.outcome.enginePath, file.fileName());
    }

    // DFR-0077: a stream that breaks a rule ends the run at once, not when
    // the engine exits; an engine run without the launcher is stopped.
    void brokenStreamEndsAtOnce()
    {
        useEngine(fake("malformed.sh"));
        int ends = 0;
        RunOutcome outcome;
        EngineProcess engine;
        connect(&engine, &EngineProcess::ended, this, [&](const RunOutcome &ended) {
            outcome = ended;
            ++ends;
        });
        engine.start(CommandProbe);
        QTRY_COMPARE_WITH_TIMEOUT(ends, 1, 5000);
        QCOMPARE(outcome.end, RunEnd::Broken);
        QTRY_VERIFY_WITH_TIMEOUT(!engine.isRunning(), 5000);
        QCoreApplication::sendPostedEvents();  // a second ended() would land here
        QCOMPARE(ends, 1);
    }

    // The same through the launcher. A root engine cannot be stopped from
    // here, so the run ends at once and the engine at its next write.
    void brokenStreamViaLauncherEndsAtOnce()
    {
        useEngine(fake("malformed.sh"));
        int ends = 0;
        RunOutcome outcome;
        EngineProcess engine(fake("pass-through.sh"));
        connect(&engine, &EngineProcess::ended, this, [&](const RunOutcome &ended) {
            outcome = ended;
            ++ends;
        });
        engine.start(CommandReport, {QStringLiteral("/")});
        QTRY_COMPARE_WITH_TIMEOUT(ends, 1, 5000);
        QCOMPARE(outcome.end, RunEnd::Broken);
    }

    // DFR-0002 § 4.3: a path reaches the engine escaped as mountinfo
    // writes it. A name that is UTF-8 stays readable; one that is not is
    // escaped above ASCII, since QProcess would turn its bytes into U+FFFD.
    // image_report checks the engine decodes it.
    void pathArgument()
    {
        QCOMPARE(EngineProcess::pathArgument(QByteArray("/mnt/games")),
                 QStringLiteral("/mnt/games"));
        QCOMPARE(EngineProcess::pathArgument(QByteArray("/mnt/sp\xc3\xa9l x")),
                 QString::fromUtf8("/mnt/sp\xc3\xa9l x"));
        QCOMPARE(EngineProcess::pathArgument(QByteArray("/mnt/a\\b")),
                 QStringLiteral("/mnt/a\\134b"));
        QCOMPARE(EngineProcess::pathArgument(QByteArray("/mnt/t\xff\xc3\xa9\\")),
                 QStringLiteral("/mnt/t\\377\\303\\251\\134"));
    }

    // Closing the window mid-report destroys the EngineProcess with the
    // engine still running and output still unread. Nothing may reach
    // the half-destroyed object: once, it corrupted the heap.
    void destroyedMidRun()
    {
        FakeHold hold;
        QVERIFY(hold.isValid());
        useEngine(fake("chatter.sh"));
        int batches = 0;
        bool destroying = false;
        int batchesWhileDestroying = 0;
        // Owned, so a failed check still stops the engine: its wait would
        // hold CTest's standard error open for 30 s.
        auto engine = std::make_unique<EngineProcess>(fake("pass-through.sh"));
        connect(engine.get(), &EngineProcess::recordsReceived, this, [&] {
            ++batches;
            if (destroying)
                ++batchesWhileDestroying;
        });
        engine->start(CommandReport, {QStringLiteral("/")});
        QVERIFY(QTest::qWaitFor([&] { return batches > 0; }, 10000));
        // No event loop from here, so the fake's burst arrives and stays
        // unread.
        QVERIFY(hold.release());
        const QDeadlineTimer deadline(10000);
        while (!hold.sent() && !deadline.hasExpired())
            QThread::msleep(5);
        QVERIFY2(hold.sent(), "the fake did not send its burst");
        destroying = true;
        engine.reset();
        QCOMPARE(batchesWhileDestroying, 0);
    }
};

QTEST_GUILESS_MAIN(TestEngineProcess)
#include "tst_engineprocess.moc"
