// EngineProcess: runs one engine command and says how it ended. Starts
// the engine plainly for probe and through pkexec for anything else
// (docs/design.md § Window), turns its output into records, and tells a
// finished run from a missing engine, a missing pkexec, refused admin
// rights, or a broken engine (docs/specs/DFR-0002-engine-protocol.md § 6).
#pragma once

#include "engine/Records.h"

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace defragr::engine {

enum class RunEnd {
    Finished,         // FINISHED arrived; its result says how the command went
    EngineMissing,    // no engine program where the window looks for one
    LauncherMissing,  // pkexec itself could not be started, so nobody was asked
    AdminRefused,     // pkexec did not run the engine: refused or blocked
    Broken,           // the engine crashed, or its stream broke a rule
    VersionMismatch,  // the engine speaks another protocol major (INV-7)
};

struct RunOutcome {
    RunEnd end = RunEnd::Broken;
    Finished finished;      // set when end is Finished
    QString enginePath;     // where the engine was looked for
};

class EngineProcess : public QObject {
    Q_OBJECT

public:
    // launcher is the program that gains admin rights; tests pass a fake.
    explicit EngineProcess(QString launcher = QStringLiteral("pkexec"),
                           QObject *parent = nullptr);
    // Lets go of a running engine, which ends at its next write after.
    ~EngineProcess() override;

    // DEFRAGR_ENGINE when set, otherwise defragr-engine beside the window.
    static QString enginePath();
    // A path as an engine argument, escaped as mountinfo writes it, so
    // bytes that are not UTF-8 survive QProcess (DFR-0002 § 4.3).
    static QString pathArgument(const QByteArray &path);

    // Runs one command. Ignored while a run is going.
    void start(Command command, const QStringList &arguments = {});
    bool isRunning() const { return m_process.state() != QProcess::NotRunning; }

signals:
    // Records decoded so far, in stream order; emitted as output arrives.
    void recordsReceived(const QList<defragr::engine::Record> &records);
    // Once per start(), after the last recordsReceived.
    void ended(const defragr::engine::RunOutcome &outcome);

private:
    void readOutput();
    void processEnded(int exitCode, QProcess::ExitStatus status);

    QString m_launcher;
    QProcess m_process;
    RecordDecoder m_decoder;
    RunOutcome m_outcome;
    bool m_viaLauncher = false;
    bool m_gotOutput = false;
};

}  // namespace defragr::engine
