// DriveList: the drives the engine's probe lists (S1), and which actions
// each allows (S12). The actions follow the engine's verdict alone and
// are never re-derived here (ADR-0002).
#pragma once

#include "engine/EngineProcess.h"
#include "engine/Records.h"
#include "model/Messages.h"

#include <QAbstractTableModel>
#include <QList>

namespace defragr::model {

class DriveList : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { ColumnDrive, ColumnSize, ColumnFileSystem, ColumnMedia, ColumnActions, ColumnCount };

    explicit DriveList(QObject *parent = nullptr);

    // Runs the probe; ends in loaded() or failed(). The list keeps its
    // old drives until a probe succeeds, and empties when one fails.
    // Ignored while a probe is going.
    void refresh();
    // From refresh() until loaded() or failed().
    bool isListing() const { return m_listing; }
    void setDrives(QList<engine::Drive> drives);

    const engine::Drive &drive(int row) const { return m_drives.at(row); }
    bool canReport(int row) const;
    bool canDefrag(int row) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

signals:
    void loaded();
    void failed(const defragr::model::Message &message);

private:
    void probeEnded(const engine::RunOutcome &outcome);

    engine::EngineProcess m_engine;
    QList<engine::Drive> m_drives;
    QList<engine::Drive> m_incoming;  // the running probe's drives so far
    bool m_listing = false;
};

}  // namespace defragr::model
