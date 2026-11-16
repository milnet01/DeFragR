#include "model/Report.h"

#include <QFile>
#include <QLocale>

#include <algorithm>
#include <cmath>
#include <utility>

namespace defragr::model {

using namespace engine;

namespace {

// "1 file" or "58,310 files".
QString count(quint64 n, const char *one, const char *many)
{
    return QStringLiteral("%1 %2").arg(QLocale().toString(n),
                                       QLatin1String(n == 1 ? one : many));
}

double rounded(double value, int decimals)
{
    const double scale = std::pow(10.0, decimals);
    return std::round(value * scale) / scale;
}

// With no trailing zeros: "2.1", "100".
QString shortest(double value)
{
    return QLocale().toString(value, 'g', QLocale::FloatingPointShortest);
}

QString percentFragmented(quint64 fragmented, quint64 files)
{
    if (fragmented == 0)
        return QStringLiteral("0%");
    const double percent = 100.0 * double(fragmented) / double(files);
    // A drive with a few fragmented files among millions is not at 0%.
    if (percent < 0.05)
        return QStringLiteral("under 0.1%");
    return shortest(rounded(percent, 1)) + QLatin1Char('%');
}

}  // namespace

QStringList summaryLines(const ReportSummary &summary)
{
    QStringList lines;
    if (summary.files == 0) {
        lines << QStringLiteral("DeFragR found no files on this drive.");
    } else {
        lines << QStringLiteral("%1 of %2 checked %3 fragmented (%4).")
                     .arg(QLocale().toString(summary.fragmentedFiles),
                          count(summary.files, "file", "files"),
                          QLatin1String(summary.fragmentedFiles == 1 ? "is" : "are"),
                          percentFragmented(summary.fragmentedFiles, summary.files));
        const double average =
            rounded(double(summary.extents) / double(summary.files), 2);
        lines << QStringLiteral("On average a file is in %1 %2.")
                     .arg(shortest(average), QLatin1String(average == 1.0 ? "piece" : "pieces"));
    }
    if (summary.skipped > 0) {
        lines << QStringLiteral("%1 could not be read, so %2 not counted.")
                     .arg(count(summary.skipped, "file or folder", "files or folders"),
                          QLatin1String(summary.skipped == 1 ? "it is" : "they are"));
    }
    return lines;
}

Report::Report(QString launcher, QObject *parent)
    : QAbstractTableModel(parent), m_engine(std::move(launcher))
{
    connect(&m_engine, &EngineProcess::recordsReceived, this, &Report::collect);
    connect(&m_engine, &EngineProcess::ended, this, &Report::runEnded);
}

void Report::run(const QByteArray &mountPoint)
{
    if (isRunning())
        return;
    clear();
    m_mountPoint = mountPoint;
    m_incomingBlockSize = 0;
    m_incomingSummary = {};
    m_incoming.clear();
    m_engine.start(CommandReport, {QFile::decodeName(mountPoint)});
}

void Report::collect(const QList<Record> &records)
{
    for (const Record &record : records) {
        if (const auto *file = std::get_if<FragmentedFile>(&record))
            m_incoming.append(*file);
        else if (const auto *summary = std::get_if<ReportSummary>(&record))
            m_incomingSummary = *summary;
        else if (const auto *hello = std::get_if<Hello>(&record))
            m_incomingBlockSize = hello->blockSize;
    }
}

void Report::runEnded(const RunOutcome &outcome)
{
    const Message problem = messages::forRun(outcome);
    if (!problem.what.isEmpty()) {
        // A broken run's figures are never shown: a message, never a guess.
        m_incoming.clear();
        emit failed(problem);
        return;
    }
    std::ranges::stable_sort(m_incoming, [](const FragmentedFile &a, const FragmentedFile &b) {
        if (a.extents != b.extents)
            return a.extents > b.extents;
        return a.sizeBlocks > b.sizeBlocks;
    });
    beginResetModel();
    m_blockSize = m_incomingBlockSize;
    m_summary = m_incomingSummary;
    m_files = std::exchange(m_incoming, {});
    endResetModel();
    emit loaded();
}

void Report::clear()
{
    beginResetModel();
    m_mountPoint.clear();
    m_blockSize = 0;
    m_summary = {};
    m_files.clear();
    endResetModel();
}

int Report::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_files.size());
}

int Report::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant Report::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_files.size() || role != Qt::DisplayRole)
        return {};
    const FragmentedFile &file = m_files.at(index.row());
    switch (index.column()) {
    case ColumnPieces:
        return QLocale().toString(file.extents);
    case ColumnSize:
        return QLocale().formattedDataSize(qint64(file.sizeBlocks * m_blockSize));
    case ColumnFile: {
        QByteArray path = m_mountPoint;
        if (!path.endsWith('/'))
            path += '/';
        return QFile::decodeName(path + file.path);
    }
    }
    return {};
}

QVariant Report::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColumnPieces:
        return QStringLiteral("Pieces");
    case ColumnSize:
        return QStringLiteral("Size");
    case ColumnFile:
        return QStringLiteral("File");
    }
    return {};
}

}  // namespace defragr::model
