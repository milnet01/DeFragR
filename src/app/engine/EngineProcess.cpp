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
        if (error != QProcess::FailedToStart)
            return;
        m_outcome.end = m_viaLauncher ? RunEnd::LauncherMissing : RunEnd::EngineMissing;
        emit ended(m_outcome);
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
    m_decoder = RecordDecoder();
    m_outcome = RunOutcome();
    m_outcome.enginePath = enginePath();
    m_gotOutput = false;
    m_command = command;
    m_helloCommand = 0;
    m_viaLauncher = command != CommandProbe;

    const QFileInfo engine(m_outcome.enginePath);
    if (!engine.isFile() || !engine.isExecutable()) {
        // Queued, so ended() always comes after start() returns.
        m_outcome.end = RunEnd::EngineMissing;
        QTimer::singleShot(0, this, [this] { emit ended(m_outcome); });
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
    if (records.isEmpty())
        return;
    for (const Record &record : records) {
        if (const auto *finished = std::get_if<Finished>(&record))
            m_outcome.finished = *finished;
        else if (const auto *hello = std::get_if<Hello>(&record))
            m_helloCommand = hello->command;
    }
    emit recordsReceived(records);
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
        m_outcome.end = RunEnd::AdminRefused;
    else if (m_decoder.error() == StreamError::VersionMismatch)
        m_outcome.end = RunEnd::VersionMismatch;
    else if (status == QProcess::CrashExit || m_decoder.error() || otherCommand)
        m_outcome.end = RunEnd::Broken;
    else
        m_outcome.end = RunEnd::Finished;
    emit ended(m_outcome);
}

}  // namespace defragr::engine
