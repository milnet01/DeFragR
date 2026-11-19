// Report: how fragmented one drive is, and its fragmented files worst
// first (S3), from the engine's report stream
// (docs/specs/DFR-0002-engine-protocol.md § 4.6). Worst means the most
// pieces; of two files in as many pieces, the bigger is worse.
#pragma once

#include "engine/EngineProcess.h"
#include "engine/Records.h"
#include "model/DriveMap.h"
#include "model/Messages.h"

#include <QAbstractTableModel>
#include <QList>
#include <QStringList>

namespace defragr::model {

// The summary's figures as short sentences, for the report's top.
QStringList summaryLines(const engine::ReportSummary &summary);

// How far a running report has got, from REPORT_PROGRESS: a whole
// percentage that stays below 100 until the report ends, since the
// estimate is not exact; -1 when there is no estimate, or no name has
// been checked yet.
int percentChecked(quint64 names, quint64 estimate);
// The same as a sentence for the running report.
QString progressLine(quint64 names, quint64 estimate);

// A percentage in the reader's own digits, such as 1.5%.
QString percentText(double percent);

// How long a report has run, such as "2 min 5 s".
QString elapsedText(qint64 seconds);
// While a report runs: how many fragmented files it has found.
QString foundLine(quint64 count);

// The heading above the file list, which says how many files the list
// left out to keep within its memory limit.
QString filesHeading(quint64 unlisted);

class Report : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { ColumnPieces, ColumnSize, ColumnFile, ColumnCount };

    // The file list's memory limit: each file's path, and kFileEntryBytes
    // more for the entry around it. A local user who can write to the
    // drive chooses how many fragmented files there are, so the list is
    // bounded; this one holds over three million files whose paths are
    // 100 bytes long.
    static constexpr qsizetype kFileEntryBytes = 64;
    static constexpr qsizetype kFileListBytes = qsizetype(512) * 1024 * 1024;

    // launcher is the program that gains admin rights; tests pass a fake,
    // and a small listBytes to reach the limit.
    explicit Report(QString launcher = QStringLiteral("pkexec"),
                    qsizetype listBytes = kFileListBytes, QObject *parent = nullptr);

    // Empties the report, then runs the engine's report on the drive
    // mounted there; ends in loaded() or failed(). Ignored while a run is
    // going.
    void run(const QByteArray &mountPoint);
    bool isRunning() const { return m_engine.isRunning(); }

    const QByteArray &mountPoint() const { return m_mountPoint; }
    const engine::ReportSummary &summary() const { return m_summary; }
    // Fragmented files found but left out of the list at its limit. The
    // list keeps the worst.
    quint64 unlisted() const { return m_unlisted; }
    const DriveMap &map() const { return m_map; }

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

signals:
    // While a run goes: the fragmented files found so far, listed or not,
    // and the path of the latest, relative to the mount point.
    void found(quint64 count, const QByteArray &latestPath);
    // While a run goes: names the engine has read, of about `estimate`.
    void progressed(quint64 names, quint64 estimate);
    void loaded();
    void failed(const defragr::model::Message &message);

private:
    void collect(const QList<engine::Record> &records);
    void keep(const engine::FragmentedFile &file);
    void runEnded(const engine::RunOutcome &outcome);
    void clear();

    engine::EngineProcess m_engine;
    QByteArray m_mountPoint;
    quint32 m_blockSize = 0;
    engine::ReportSummary m_summary;
    QList<engine::FragmentedFile> m_files;
    quint64 m_unlisted = 0;
    DriveMap m_map;
    qsizetype m_listBytes;
    // The running report's records so far; shown only once it succeeds.
    quint32 m_incomingBlockSize = 0;
    engine::ReportSummary m_incomingSummary;
    QList<engine::FragmentedFile> m_incomingFiles;
    qsizetype m_incomingBytes = 0;  // counted as kFileListBytes says
    bool m_incomingIsHeap = false;  // least bad first, once at the limit
    quint64 m_incomingUnlisted = 0;
    DriveMap m_incomingMap;
};

}  // namespace defragr::model
