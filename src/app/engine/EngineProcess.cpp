#include "engine/EngineProcess.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <utility>

namespace defragr::engine {

namespace {

// pkexec's own exit codes, before any engine runs: 126 when the user
// dismissed the password dialog, 127 when not authorised or on its error.
// An error includes failing to run the engine once authorised, as when
// SELinux denies it; the exit code alone cannot tell that from a refusal.
constexpr int kPkexecDismissed = 126;
constexpr int kPkexecRefused = 127;

// The word on the engine's command line for each command (§ 4.3).
QString commandWord(Command command)
{
    switch (command) {
    case CommandProbe:
        return QStringLiteral("probe");
    case CommandReport:
        return QStringLiteral("report");
    case CommandSelftestCrc32c:
        return QStringLiteral("selftest-crc32c");
    case CommandSelftest:
        return QStringLiteral("selftest-records");
    case CommandUnknown:
        break;
    }
    return {};
}

}  // namespace

EngineProcess::EngineProcess(QString launcher, QObject *parent)
    : QObject(parent), m_launcher(std::move(launcher))
{
    // Standard error is for a crash only (design.md § Errors); let it
    // reach the terminal rather than read it.
    m_process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &EngineProcess::readOutput);
    connect(&m_process, &QProcess::finished, this, &EngineProcess::processEnded);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        // FailedToStart is the one error after which finished never comes.
        // start() found the engine there and executable, so if it is the
        // engine that failed, it is not missing: it would not start, as on
        // a noexec mount or when it is not a program at all.
        if (error != QProcess::FailedToStart)
            return;
        endLater(m_viaLauncher ? RunEnd::LauncherMissing : RunEnd::EngineUnstartable);
    });
}

EngineProcess::~EngineProcess()
{
    // ~QProcess waits for a running engine, up to 30 s, since one running
    // as root cannot be killed from here. It hands on output as it waits,
    // by when this object's other members are gone, so stop listening
    // first. Once ~QProcess closes the pipes, the engine's next write
    // fails, and that ends it (docs/design.md § Errors).
    disconnect(&m_process, nullptr, this, nullptr);
    // The engine does not read yet; once it does, this is cancel (DFR-0002
    // § 4.8, DFR-0023).
    m_process.closeWriteChannel();
}

QString EngineProcess::enginePath()
{
    const QString fromEnvironment = qEnvironmentVariable("DEFRAGR_ENGINE");
    if (!fromEnvironment.isEmpty())
        return fromEnvironment;
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("defragr-engine"));
}

QString EngineProcess::pathArgument(const QByteArray &path)
{
    // QProcess turns each argument into bytes with QFile::encodeName.
    // Where that gives the path back, only a backslash is escaped, so the
    // password prompt still shows the name as written.
    const bool asText = QFile::encodeName(QFile::decodeName(path)) == path;
    QByteArray escaped;
    for (const char c : path) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte == '\\' || (!asText && byte >= 0x80)) {
            escaped += '\\';
            escaped += QByteArray::number(byte, 8).rightJustified(3, '0');
        } else {
            escaped += c;
        }
    }
    return QFile::decodeName(escaped);
}

void EngineProcess::start(Command command, const QStringList &arguments)
{
    if (isRunning())
        return;
    m_ended = false;
    m_decoder = RecordDecoder();
    m_outcome = RunOutcome();
    m_outcome.enginePath = enginePath();
    m_gotOutput = false;
    m_command = command;
    m_helloCommand = 0;
    m_viaLauncher = command != CommandProbe;

    const QFileInfo engine(m_outcome.enginePath);
    if (!engine.isFile() || !engine.isExecutable()) {
        endLater(RunEnd::EngineMissing);
        return;
    }

    QStringList engineLine{commandWord(command)};
    engineLine += arguments;
    if (m_viaLauncher)
        m_process.start(m_launcher, QStringList{m_outcome.enginePath} + engineLine);
    else
        m_process.start(m_outcome.enginePath, engineLine);
}

void EngineProcess::readOutput()
{
    const QByteArray bytes = m_process.readAllStandardOutput();
    if (bytes.isEmpty())
        return;
    m_gotOutput = true;
    m_decoder.feed(bytes);
    const QList<Record> records = m_decoder.takeRecords();
    for (const Record &record : records) {
        if (const auto *finished = std::get_if<Finished>(&record))
            m_outcome.finished = *finished;
        else if (const auto *hello = std::get_if<Hello>(&record))
            m_helloCommand = hello->command;
    }
    if (!records.isEmpty())
        emit recordsReceived(records);
    if (!m_decoder.error() || m_ended)
        return;
    // Nothing after a broken record can be trusted, so the run ends here
    // rather than when the engine exits: a root report would otherwise
    // walk the whole drive under a running screen. An engine run as this
    // user is stopped; one run as root ends at its next write once the
    // pipes close.
    if (m_viaLauncher)
        m_process.closeWriteChannel();
    else
        m_process.kill();
    end(m_decoder.error() == StreamError::VersionMismatch ? RunEnd::VersionMismatch
                                                          : RunEnd::Broken);
}

void EngineProcess::processEnded(int exitCode, QProcess::ExitStatus status)
{
    readOutput();
    m_decoder.finish();

    const bool launcherSaidNo = m_viaLauncher && !m_gotOutput && status == QProcess::NormalExit
                                && (exitCode == kPkexecDismissed || exitCode == kPkexecRefused);
    // A stream for another command answers nothing that was asked. Bad
    // arguments are the one exception: HELLO command 0 with USAGE (§ 4.3).
    const bool otherCommand = m_helloCommand != m_command
                              && !(m_helloCommand == CommandUnknown
                                   && m_outcome.finished.result == ResultUsage);
    if (launcherSaidNo)
        end(RunEnd::AdminRefused);
    else if (m_decoder.error() == StreamError::VersionMismatch)
        end(RunEnd::VersionMismatch);
    else if (status == QProcess::CrashExit || m_decoder.error() || otherCommand)
        end(RunEnd::Broken);
    else
        end(RunEnd::Finished);
}

void EngineProcess::end(RunEnd how)
{
    if (m_ended)
        return;
    m_ended = true;
    m_outcome.end = how;
    emit ended(m_outcome);
}

void EngineProcess::endLater(RunEnd how)
{
    // Queued, so ended() always comes after start() returns; until then
    // the run counts as going.
    m_endQueued = true;
    QTimer::singleShot(0, this, [this, how] {
        m_endQueued = false;
        end(how);
    });
}

}  // namespace defragr::engine
