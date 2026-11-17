#include "model/DriveList.h"

#include <QFile>
#include <QLocale>

#include <utility>

namespace defragr::model {

using namespace engine;

DriveList::DriveList(QObject *parent) : QAbstractTableModel(parent)
{
    connect(&m_engine, &EngineProcess::recordsReceived, this, [this](const QList<Record> &records) {
        for (const Record &record : records) {
            if (const auto *drive = std::get_if<Drive>(&record))
                m_incoming.append(*drive);
        }
    });
    connect(&m_engine, &EngineProcess::ended, this, &DriveList::probeEnded);
}

void DriveList::refresh()
{
    // Not m_engine.isRunning(): a missing engine's end is queued, with
    // no process running.
    if (m_listing)
        return;
    m_listing = true;
    m_incoming.clear();
    m_engine.start(CommandProbe);
}

void DriveList::probeEnded(const RunOutcome &outcome)
{
    m_listing = false;
    const Message problem = messages::forRun(outcome);
    if (problem.what.isEmpty()) {
        setDrives(std::exchange(m_incoming, {}));
        emit loaded();
        return;
    }
    // A broken run's drives are never shown: a message, never a guess.
    m_incoming.clear();
    setDrives({});
    emit failed(problem);
}

void DriveList::setDrives(QList<Drive> drives)
{
    beginResetModel();
    m_drives = std::move(drives);
    endResetModel();
}

bool DriveList::canReport(int row) const
{
    const quint8 verdict = m_drives.at(row).verdict;
    return verdict == VerdictReportOnly || verdict == VerdictReportAndDefrag;
}

bool DriveList::canDefrag(int row) const
{
    return m_drives.at(row).verdict == VerdictReportAndDefrag;
}

int DriveList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_drives.size());
}

int DriveList::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DriveList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_drives.size())
        return {};
    const Drive &drive = m_drives.at(index.row());
    if (role == Qt::ToolTipRole) {
        // A filesystem name is anybody's to choose. Escaped inside a
        // paragraph, the tooltip shows it as text, never as markup.
        const QString why = messages::whyNoDefrag(drive);
        return why.isEmpty() ? why : QStringLiteral("<p>%1</p>").arg(why.toHtmlEscaped());
    }
    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
    case ColumnDrive:
        return QFile::decodeName(drive.mountPoint);
    case ColumnSize:
        return QLocale().formattedDataSize(qint64(drive.totalBlocks * drive.blockSize));
    case ColumnFileSystem:
        return QFile::decodeName(drive.fsType);
    case ColumnMedia:
        return messages::media(drive.media);
    case ColumnActions:
        return messages::verdict(drive.verdict);
    }
    return {};
}

QVariant DriveList::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColumnDrive:
        return tr("Drive");
    case ColumnSize:
        return tr("Size");
    case ColumnFileSystem:
        return tr("File system");
    case ColumnMedia:
        return tr("Type");
    case ColumnActions:
        //: Heads a column of what DeFragR can do with each drive, such as
        //: "Report and defrag".
        return tr("DeFragR can");
    }
    return {};
}

}  // namespace defragr::model
