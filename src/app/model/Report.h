// Report: how fragmented one drive is, and its fragmented files worst
// first (S3), from the engine's report stream
// (docs/specs/DFR-0002-engine-protocol.md § 4.6). Worst means the most
// pieces; of two files in as many pieces, the bigger is worse.
#pragma once

#include "engine/EngineProcess.h"
#include "engine/Records.h"
#include "model/Messages.h"

#include <QAbstractTableModel>
#include <QList>
#include <QStringList>

namespace defragr::model {

// The summary's figures as short sentences, for the report's top.
QStringList summaryLines(const engine::ReportSummary &summary);

// How far a running report has got, from REPORT_PROGRESS: a whole
// percentage that stays below 100 until the report ends, since the
// estimate is not exact; -1 when there is no estimate.
int percentChecked(quint64 names, quint64 estimate);
// The same as a sentence for the running report.
QString progressLine(quint64 names, quint64 estimate);

class Report : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { ColumnPieces, ColumnSize, ColumnFile, ColumnCount };

    // launcher is the program that gains admin rights; tests pass a fake.
    explicit Report(QString launcher = QStringLiteral("pkexec"), QObject *parent = nullptr);

    // Empties the report, then runs the engine's report on the drive
    // mounted there; ends in loaded() or failed(). Ignored while a run is
    // going.
    void run(const QByteArray &mountPoint);
    bool isRunning() const { return m_engine.isRunning(); }

    const QByteArray &mountPoint() const { return m_mountPoint; }
    const engine::ReportSummary &summary() const { return m_summary; }

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

signals:
    // While a run goes: the fragmented files found so far, and the path
    // of the latest, relative to the mount point.
    void found(quint64 count, const QByteArray &latestPath);
    // While a run goes: names the engine has read, of about `estimate`.
    void progressed(quint64 names, quint64 estimate);
    void loaded();
    void failed(const defragr::model::Message &message);

private:
    void collect(const QList<engine::Record> &records);
    void runEnded(const engine::RunOutcome &outcome);
    void clear();

    engine::EngineProcess m_engine;
    QByteArray m_mountPoint;
    quint32 m_blockSize = 0;
    engine::ReportSummary m_summary;
    QList<engine::FragmentedFile> m_files;
    // The running report's records so far; shown only once it succeeds.
    quint32 m_incomingBlockSize = 0;
    engine::ReportSummary m_incomingSummary;
    QList<engine::FragmentedFile> m_incoming;
};

}  // namespace defragr::model
