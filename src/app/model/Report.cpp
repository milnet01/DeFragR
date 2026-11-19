#include "model/Report.h"

#include <QLocale>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace defragr::model {

using namespace engine;

namespace {

double rounded(double value, int decimals)
{
    const double scale = std::pow(10.0, decimals);
    return std::round(value * scale) / scale;
}

// S3's order: the most pieces first; of two in as many pieces, the bigger.
bool worse(const FragmentedFile &a, const FragmentedFile &b)
{
    if (a.extents != b.extents)
        return a.extents > b.extents;
    return a.sizeBlocks > b.sizeBlocks;
}

qsizetype bytesOf(const FragmentedFile &file)
{
    return file.path.size() + Report::kFileEntryBytes;
}

// With no trailing zeros: "2.1", "100".
QString shortest(double value)
{
    return QLocale().toString(value, 'g', QLocale::FloatingPointShortest);
}

QString percentText(double percent)
{
    //: A percentage, such as 1.5%.
    return Report::tr("%1%").arg(shortest(percent));
}

QString percentFragmented(quint64 fragmented, quint64 files)
{
    if (fragmented == 0)
        return percentText(0);
    const double percent = 100.0 * double(fragmented) / double(files);
    // A drive with a few fragmented files among millions is not at 0%.
    if (percent < 0.05)
        //: %1 is the smallest percentage shown, 0.1%.
        return Report::tr("under %1").arg(percentText(0.1));
    // Nor is one with a few whole files among millions at 100%.
    if (fragmented < files && rounded(percent, 1) >= 100)
        //: %1 is the largest percentage shown short of all, 99.9%.
        return Report::tr("over %1").arg(percentText(99.9));
    return percentText(rounded(percent, 1));
}

}  // namespace

QStringList summaryLines(const ReportSummary &summary)
{
    QStringList lines;
    if (summary.files == 0) {
        lines << Report::tr("DeFragR found no files on this drive.");
    } else {
        //: %1 is the number of files checked. It is %2 in "3 of [200 files]
        //: checked are fragmented", so use the form that fits there.
        const QString checked =
            Report::tr("%1 file(s)", nullptr, messages::pluralOf(summary.files))
                .arg(QLocale().toString(summary.files));
        //: %1 is the number of fragmented files, %2 the files checked, such
        //: as "200 files", and %3 a percentage.
        lines << Report::tr("%1 of %2 checked is fragmented (%3).", nullptr,
                            messages::pluralOf(summary.fragmentedFiles))
                     .arg(QLocale().toString(summary.fragmentedFiles), checked,
                          percentFragmented(summary.fragmentedFiles, summary.files));
        const double average =
            rounded(double(summary.extents) / double(summary.files), 2);
        // Plural forms are for whole numbers; a fraction has its own sentence.
        if (average == std::floor(average))
            lines << Report::tr("On average a file is in %1 piece(s).", nullptr, int(average))
                         .arg(shortest(average));
        else
            //: %1 has decimals, such as 1.25.
            lines << Report::tr("On average a file is in %1 pieces.").arg(shortest(average));
    }
    if (summary.skipped > 0) {
        lines << Report::tr("%1 file(s) or folder(s) could not be read, so it is (they are) "
                            "not counted.",
                            nullptr, messages::pluralOf(summary.skipped))
                     .arg(QLocale().toString(summary.skipped));
    }
    return lines;
}

int percentChecked(quint64 names, quint64 estimate)
{
    if (estimate == 0)
        return -1;
    if (names >= estimate)
        return 99;
    // Doubles, so a huge count cannot overflow; min, as a quotient near 1
    // can round up to it.
    return std::min(99, int(100.0 * double(names) / double(estimate)));
}

QString progressLine(quint64 names, quint64 estimate)
{
    const int percent = percentChecked(names, estimate);
    if (percent < 0)
        return Report::tr("Files and folders checked so far: %1.")
            .arg(QLocale().toString(names));
    //: %3 is a percentage, such as 50%.
    return Report::tr("Checked %1 of about %2 file(s) and folder(s) (%3).", nullptr,
                      messages::pluralOf(estimate))
        .arg(QLocale().toString(names), QLocale().toString(estimate), percentText(percent));
}

QString elapsedText(qint64 seconds)
{
    // QString::arg(int) writes ASCII digits, so each number is QLocale's.
    const auto number = [](qint64 n) { return QLocale().toString(n); };
    if (seconds >= 3600)
        //: Hours and minutes, short.
        return Report::tr("%1 h %2 min").arg(number(seconds / 3600), number(seconds % 3600 / 60));
    if (seconds >= 60)
        //: Minutes and seconds, short.
        return Report::tr("%1 min %2 s").arg(number(seconds / 60), number(seconds % 60));
    //: Seconds, short.
    return Report::tr("%1 s").arg(number(seconds));
}

QString foundLine(quint64 count)
{
    return Report::tr("Found %1 fragmented file(s) so far.", nullptr, messages::pluralOf(count))
        .arg(QLocale().toString(count));
}

QString filesHeading(quint64 unlisted)
{
    if (unlisted == 0)
        return Report::tr("Fragmented files, worst first:");
    //: %1 is a count of files, more than zero.
    return Report::tr("Fragmented files, worst first. To save memory, the %1 least "
                      "fragmented file(s) are not listed:", nullptr, messages::pluralOf(unlisted))
        .arg(QLocale().toString(unlisted));
}

Report::Report(QString launcher, qsizetype listBytes, QObject *parent)
    : QAbstractTableModel(parent), m_engine(std::move(launcher)), m_listBytes(listBytes)
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
    m_incomingFiles.clear();
    m_incomingBytes = 0;
    m_incomingIsHeap = false;
    m_incomingUnlisted = 0;
    m_incomingMap.clear();
    m_engine.start(CommandReport, {EngineProcess::pathArgument(mountPoint)});
}

void Report::collect(const QList<Record> &records)
{
    const FragmentedFile *latest = nullptr;
    std::optional<ReportProgress> progress;
    for (const Record &record : records) {
        if (const auto *file = std::get_if<FragmentedFile>(&record)) {
            keep(*file);
            latest = file;
        } else if (const auto *latest = std::get_if<ReportProgress>(&record))
            progress = *latest;
        else if (const auto *runs = std::get_if<MapRuns>(&record))
            m_incomingMap.add(*runs);
        else if (const auto *summary = std::get_if<ReportSummary>(&record))
            m_incomingSummary = *summary;
        else if (const auto *hello = std::get_if<Hello>(&record))
            m_incomingBlockSize = hello->blockSize;
    }
    if (latest)
        emit found(quint64(m_incomingFiles.size()) + m_incomingUnlisted, latest->path);
    if (progress)
        emit progressed(progress->names, progress->estimate);
}

// Lists the file. At the limit, the list becomes a heap with its least
// bad file on top, and drops that file until the list fits again.
void Report::keep(const FragmentedFile &file)
{
    m_incomingFiles.append(file);
    m_incomingBytes += bytesOf(file);
    if (m_incomingIsHeap) {
        std::ranges::push_heap(m_incomingFiles, worse);
    } else if (m_incomingBytes > m_listBytes) {
        std::ranges::make_heap(m_incomingFiles, worse);
        m_incomingIsHeap = true;
    }
    while (m_incomingBytes > m_listBytes && !m_incomingFiles.isEmpty()) {
        std::ranges::pop_heap(m_incomingFiles, worse);
        m_incomingBytes -= bytesOf(m_incomingFiles.last());
        m_incomingFiles.removeLast();
        ++m_incomingUnlisted;
    }
}

void Report::runEnded(const RunOutcome &outcome)
{
    const Message problem = messages::forRun(outcome);
    if (!problem.what.isEmpty()) {
        // A broken run's figures are never shown: a message, never a guess.
        m_incomingFiles.clear();
        m_incomingBytes = 0;
        m_incomingIsHeap = false;
        m_incomingUnlisted = 0;
        m_incomingMap.clear();
        emit failed(problem);
        return;
    }
    std::ranges::stable_sort(m_incomingFiles, worse);
    beginResetModel();
    m_blockSize = m_incomingBlockSize;
    m_summary = m_incomingSummary;
    m_files = std::exchange(m_incomingFiles, {});
    m_unlisted = std::exchange(m_incomingUnlisted, 0);
    m_incomingBytes = 0;
    m_incomingIsHeap = false;
    m_map = std::exchange(m_incomingMap, {});
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
    m_unlisted = 0;
    m_map.clear();
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
        return messages::path(path + file.path);
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
        return tr("Pieces");
    case ColumnSize:
        return tr("Size");
    case ColumnFile:
        return tr("File");
    }
    return {};
}

}  // namespace defragr::model
