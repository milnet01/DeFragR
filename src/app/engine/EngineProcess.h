// EngineProcess: runs one engine command and says how it ended. Starts
// the engine plainly for probe and through pkexec for anything else
// (docs/design.md § Window), turns its output into records, and tells a
// finished run from a missing engine, a missing pkexec, refused admin
// rights, or a broken engine (docs/specs/DFR-0002-engine-protocol.md § 6).
#pragma once

#include "engine/Records.h"
#include "engine/RunLog.h"

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace defragr::engine {

enum class RunEnd {
    Finished,         // FINISHED arrived; its result says how the command went
    EngineMissing,    // no engine program where the window looks for one
    EngineUnstartable,  // the engine program is there, but would not start
    LauncherMissing,  // pkexec itself could not be started, so nobody was asked
    AdminRefused,     // pkexec did not run the engine: refused or blocked
    Broken,           // the engine crashed, its stream broke a rule, or it
                      // answered a command other than the one started
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
    // Lets go of a running engine. One run without the launcher is
    // stopped at once. One run as root cannot be stopped from here, so
    // this waits up to 30 s for it to end at its next write.
    ~EngineProcess() override;

    // DEFRAGR_ENGINE when set, otherwise defragr-engine beside the window.
    static QString enginePath();
    // A path as an engine argument, escaped as mountinfo writes it, so
    // bytes that are not UTF-8 survive QProcess (DFR-0002 § 4.3).
    static QString pathArgument(const QByteArray &path);

    // Runs one command. Ignored while a run is going.
    void start(Command command, const QStringList &arguments = {});
    // From start() until ended() has come and the engine has exited.
    bool isRunning() const
    {
        return m_endQueued || m_process.state() != QProcess::NotRunning;
    }

signals:
    // Records decoded so far, in stream order; emitted as output arrives.
    void recordsReceived(const QList<defragr::engine::Record> &records);
    // Once per start(), after the last recordsReceived. A stream that
    // breaks a rule ends the run there, before the engine exits.
    void ended(const defragr::engine::RunOutcome &outcome);

private:
    void readOutput();
    void processEnded(int exitCode, QProcess::ExitStatus status);
    // Emits ended() once per run; later calls do nothing.
    void end(RunEnd how);
    // Emits ended() once control is back in the event loop.
    void endLater(RunEnd how);

    QString m_launcher;
    QProcess m_process;
    RecordDecoder m_decoder;
    RecordNotes m_notes;  // what the developers' log says of this run
    RunOutcome m_outcome;
    Command m_command = CommandUnknown;  // the command start() was given
    quint32 m_helloCommand = 0;          // the command HELLO says it answers
    bool m_viaLauncher = false;
    bool m_gotOutput = false;
    bool m_endQueued = false;  // endLater() has not yet emitted
    bool m_ended = false;      // this run's ended() has been emitted
};

}  // namespace defragr::engine
