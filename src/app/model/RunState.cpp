#include "model/RunState.h"

#include "model/Report.h"

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace defragr::model {

using namespace engine;

std::optional<QByteArray> folderOnDrive(const QByteArray &mountPoint, const QString &chosen)
{
    if (mountPoint.isEmpty() || !QDir::isAbsolutePath(chosen))
        return std::nullopt;
    // cleanPath works on the name alone: it drops "." and a doubled or
    // trailing "/", and folds ".." into the name before it.
    const QByteArray folder = QFile::encodeName(QDir::cleanPath(chosen));
    QByteArray top = mountPoint;
    while (top.size() > 1 && top.endsWith('/'))
        top.chop(1);
    if (folder == top)
        return QByteArray();
    // "/mnt/GamesOld" starts with "/mnt/Games" and is not inside it.
    const QByteArray inside = top.endsWith('/') ? top : top + '/';
    if (!folder.startsWith(inside))
        return std::nullopt;
    return folder.mid(inside.size());
}

QStringList countLines(const RunState::Counts &counts)
{
    const auto number = [](quint64 n) { return QLocale().toString(n); };
    QStringList lines;
    if (counts.joined > 0)
        //: %1 is a count of files, more than zero.
        lines += RunState::tr("%1 file(s) moved into fewer pieces.", nullptr, messages::pluralOf(counts.joined))
                     .arg(number(counts.joined));
    if (counts.left > 0)
        //: %1 is a count of files, more than zero.
        lines += RunState::tr("%1 file(s) left as they were: the drive has no free space "
                              "that would hold them in fewer pieces.",
                              nullptr, messages::pluralOf(counts.left))
                     .arg(number(counts.left));
    if (counts.skipped > 0)
        //: %1 is a count of files, more than zero.
        lines += RunState::tr("%1 file(s) skipped.", nullptr, messages::pluralOf(counts.skipped))
                     .arg(number(counts.skipped));
    if (lines.isEmpty())
        lines += RunState::tr("No file in pieces found yet.");
    return lines;
}

int percentDone(const DefragProgress &work)
{
    // Files, where the blocks say nothing.
    const quint64 done = work.blocksTotal > 0 ? work.blocksDone : work.filesDone;
    const quint64 total = work.blocksTotal > 0 ? work.blocksTotal : work.filesTotal;
    if (done >= total)
        return 100;
    // Doubles, so a huge count cannot overflow.
    return std::min(100, int(100.0 * double(done) / double(total)));
}

QString workLine(const DefragProgress &work)
{
    if (work.filesTotal == 0)
        return RunState::tr("No file in this folder is in pieces.");
    //: %1 and %2 count files, %3 is a percentage, such as 50%. The
    //: plural follows %2.
    return RunState::tr("Finished with %1 of %2 file(s) in pieces (%3).", nullptr,
                        messages::pluralOf(work.filesTotal))
        .arg(QLocale().toString(std::min(work.filesDone, work.filesTotal)),
             QLocale().toString(work.filesTotal), percentText(percentDone(work)));
}

RunState::RunState(QString launcher, QObject *parent)
    : QObject(parent), m_engine(std::move(launcher))
{
    connect(&m_engine, &EngineProcess::recordsReceived, this, &RunState::collect);
    connect(&m_engine, &EngineProcess::ended, this, &RunState::engineEnded);
}

void RunState::setMap(const QByteArray &mountPoint, const DriveMap &map)
{
    if (isRunning() || mountPoint.isEmpty() || map.isEmpty())
        return;
    m_map = map;
    m_mapMount = mountPoint;
}

bool RunState::hasMapOf(const QByteArray &mountPoint) const
{
    return !m_mapMount.isEmpty() && m_mapMount == mountPoint;
}

void RunState::start(const QByteArray &mountPoint, const QByteArray &folder)
{
    if (isRunning() || m_engine.isRunning())
        return;
    m_mountPoint = mountPoint;
    m_folder = folder;
    m_counts = {};
    m_work.reset();
    m_latestPath.clear();
    m_skipped.clear();
    m_problem = {};
    QStringList arguments{EngineProcess::pathArgument(mountPoint),
                          EngineProcess::pathArgument(folder)};
    if (hasMapOf(mountPoint)) {
        setStage(Stage::Defragging);
    } else {
        // The engine sends the drive's map first, in the same run, so
        // admin rights are asked for once (§ 4.12).
        m_map.clear();
        m_mapMount.clear();
        m_incomingMap.clear();
        arguments += QStringLiteral("map");
        setStage(Stage::ReadingMap);
    }
    m_engine.start(CommandDefrag, arguments);
}

void RunState::collect(const QList<Record> &records)
{
    std::optional<ReportProgress> progress;
    bool any = false;
    for (const Record &record : records) {
        if (const auto *hello = std::get_if<Hello>(&record))
            m_blockSize = hello->blockSize;
        if (m_stage == Stage::ReadingMap) {
            if (const auto *runs = std::get_if<MapRuns>(&record)) {
                m_incomingMap.add(*runs);
            } else if (const auto *latest = std::get_if<ReportProgress>(&record)) {
                progress = *latest;
            } else if (std::holds_alternative<MapDone>(record)) {
                m_map = std::exchange(m_incomingMap, {});
                m_mapMount = m_map.isEmpty() ? QByteArray() : m_mountPoint;
                setStage(Stage::Defragging);
            }
            continue;
        }
        if (m_stage != Stage::Defragging)
            return;
        if (const auto *move = std::get_if<BlocksMoved>(&record)) {
            m_map.move(*move);
            for (const MapRun &run : move->to)
                m_counts.blocksMoved += run.length;
            emit blocksMoved(*move);
            any = true;
        } else if (const auto *file = std::get_if<FileDefragged>(&record)) {
            m_map.endTurn(fileState(file->extentsAfter, file->movedBlocks, m_blockSize));
            ++(file->movedBlocks > 0 ? m_counts.joined : m_counts.left);
            m_latestPath = file->path;
            any = true;
        } else if (const auto *skip = std::get_if<FileSkipped>(&record)) {
            // Blocks moved before a skip stay moved (§ 4.12, stage 9), and
            // the file is not known to be in one piece.
            m_map.endTurn(CellState::Fragmented);
            ++m_counts.skipped;
            if (m_skipped.size() < kSkippedListed)
                m_skipped.append(*skip);
            m_latestPath = skip->path;
            any = true;
        } else if (const auto *work = std::get_if<DefragProgress>(&record)) {
            m_work = *work;
            any = true;
        }
    }
    if (progress)
        emit mapProgressed(progress->names, progress->estimate);
    if (any)
        emit changed();
}

void RunState::engineEnded(const RunOutcome &outcome)
{
    Message problem = messages::forRun(outcome);
    if (m_stage == Stage::ReadingMap) {
        // The run ended before its map was whole, and a part of a map is
        // never shown: a message, never a guess. A run that says OK with
        // no map broke the protocol.
        m_incomingMap.clear();
        if (problem.what.isEmpty()) {
            RunOutcome broken = outcome;
            broken.end = RunEnd::Broken;
            problem = messages::forRun(broken);
        }
        fail(problem);
        return;
    }
    if (m_stage != Stage::Defragging)
        return;
    // Every record before FINISHED was true, whatever its result, so the
    // map still follows the drive. After any other end it may not.
    if (outcome.end != RunEnd::Finished) {
        m_map.clear();
        m_mapMount.clear();
    } else {
        // A stream may end in mid-turn, as on CHECKSUM_MISMATCH's last skip.
        m_map.endTurn(CellState::Fragmented);
    }
    if (!problem.what.isEmpty()) {
        fail(problem);
        return;
    }
    setStage(Stage::Done);
    emit ended();
}

void RunState::setStage(Stage stage)
{
    if (m_stage == stage)
        return;
    m_stage = stage;
    emit stageChanged();
}

void RunState::fail(const Message &problem)
{
    m_problem = problem;
    setStage(Stage::Failed);
    emit ended();
}

}  // namespace defragr::model
