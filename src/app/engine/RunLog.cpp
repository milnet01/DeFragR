#include "engine/RunLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

namespace defragr::engine {

Q_LOGGING_CATEGORY(runLog, "defragr.run", QtWarningMsg)

namespace {

// One file, one writer at a time: Qt calls the handler from any thread.
QMutex g_mutex;
QFile g_file;
QtMessageHandler g_before = nullptr;

char levelLetter(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return 'D';
    case QtInfoMsg:
        return 'I';
    case QtWarningMsg:
        return 'W';
    case QtCriticalMsg:
        return 'C';
    case QtFatalMsg:
        return 'F';
    }
    return '?';
}

void writeMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        const QMutexLocker locker(&g_mutex);
        if (g_file.isOpen() && g_file.size() < kRunLogBytes) {
            const QString line =
                QStringLiteral("%1 %2 %3: %4\n")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                         QString(QLatin1Char(levelLetter(type))),
                         QString::fromUtf8(context.category), message);
            g_file.write(line.toUtf8());
            // At once: the line before a crash is the one wanted.
            g_file.flush();
        }
    }
    if (g_before)
        g_before(type, context, message);
}

}  // namespace

QString runLogFolder()
{
    const QString state = qEnvironmentVariable("XDG_STATE_HOME");
    const QString base = state.isEmpty() ? QDir::homePath() + QStringLiteral("/.local/state") : state;
    return base + QStringLiteral("/defragr");
}

bool startRunLog(const QString &folder)
{
    const QMutexLocker locker(&g_mutex);
    if (g_file.isOpen() || !QDir().mkpath(folder))
        return false;
    const QString path = QDir(folder).filePath(QStringLiteral("defragr.log"));
    if (QFileInfo(path).size() > kRunLogBytes) {
        const QString older = path + QStringLiteral(".1");
        QFile::remove(older);
        QFile::rename(path, older);
    }
    g_file.setFileName(path);
    if (!g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;
    // The user's own, like the names in it.
    g_file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QLoggingCategory::setFilterRules(QStringLiteral("defragr.run.info=true"));
    g_before = qInstallMessageHandler(writeMessage);
    return true;
}

void stopRunLog()
{
    const QMutexLocker locker(&g_mutex);
    if (!g_file.isOpen())
        return;
    qInstallMessageHandler(g_before);
    g_before = nullptr;
    QLoggingCategory::setFilterRules(QString());
    g_file.close();
}

bool RecordNotes::percentChanged(quint64 done, quint64 total, int *last)
{
    // With no total there is no percentage, and each record is noted.
    const int percent = total == 0 ? -1 : int(100.0 * double(done) / double(total));
    if (percent >= 0 && percent == *last)
        return false;
    *last = percent;
    return true;
}

QString RecordNotes::note(const Record &record)
{
    if (const auto *hello = std::get_if<Hello>(&record))
        return QStringLiteral("HELLO %1.%2 command %3 block size %4")
            .arg(hello->major)
            .arg(hello->minor)
            .arg(hello->command)
            .arg(hello->blockSize);
    if (const auto *finished = std::get_if<Finished>(&record))
        return QStringLiteral("FINISH result %1 errno %2").arg(finished->result).arg(finished->errnum);
    if (const auto *progress = std::get_if<ReportProgress>(&record)) {
        if (!percentChanged(progress->names, progress->estimate, &m_reportPercent))
            return {};
        return QStringLiteral("REPORT_PROGRESS names %1 of about %2")
            .arg(progress->names)
            .arg(progress->estimate);
    }
    if (const auto *work = std::get_if<DefragProgress>(&record)) {
        if (!percentChanged(work->blocksDone, work->blocksTotal, &m_defragPercent))
            return {};
        return QStringLiteral("DEFRAG_PROGRESS files %1 of %2, blocks %3 of %4")
            .arg(work->filesDone)
            .arg(work->filesTotal)
            .arg(work->blocksDone)
            .arg(work->blocksTotal);
    }
    if (const auto *skipped = std::get_if<FileSkipped>(&record))
        return QStringLiteral("FILE_SKIPPED stage %1 errno %2 %3")
            .arg(skipped->stage)
            .arg(skipped->errnum)
            .arg(QString::fromUtf8(skipped->path));
    if (std::holds_alternative<MapDone>(record))
        return QStringLiteral("MAP_DONE");
    return {};
}

}  // namespace defragr::engine
