// DefragView: one folder being defragged (S4, S7) — what the run is doing,
// the drive's map with each move playing on it, how many files were
// joined up, left or skipped, and at the end the skipped files with why
// (docs/specs/DFR-0018-window-folder-defrag.md § 4.4) — with a way back
// to the drive list once the run is over.
#pragma once

#include "model/RunState.h"

#include <QElapsedTimer>
#include <QStandardItemModel>
#include <QTimer>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QTableView;

namespace defragr::ui {

class DriveMapView;
class Theme;

class DefragView : public QWidget {
    Q_OBJECT

public:
    explicit DefragView(const Theme &theme, QWidget *parent = nullptr);

    // A map of the drive mounted there, from a report, so a defrag of
    // that drive need not read one. Ignored while a run is going.
    void setMap(const QByteArray &mountPoint, const model::DriveMap &map);
    // Defrags `folder`, written as model::folderOnDrive gives it, on the
    // drive mounted at mountPoint.
    void start(const QByteArray &mountPoint, const QByteArray &folder);
    bool isRunning() const { return m_run.isRunning(); }

signals:
    void backRequested();

private:
    void showStage();
    void showMapProgress(quint64 names, quint64 estimate);
    void showEnd();
    // Each second: how long it has run, and what it has done since.
    void showTick();
    void showCounts();
    // The bar and its sentence, once the engine has counted the work.
    void showWork();

    model::RunState m_run;
    QLabel *m_heading = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_checked = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_counts = nullptr;
    QLabel *m_latest = nullptr;
    QLabel *m_elapsed = nullptr;
    QElapsedTimer m_clock;
    QTimer m_tick;
    QWidget *m_mapKey = nullptr;
    DriveMapView *m_map = nullptr;
    QLabel *m_skippedHeading = nullptr;
    QStandardItemModel m_skipped;
    QTableView *m_skippedTable = nullptr;
    QPushButton *m_back = nullptr;
};

}  // namespace defragr::ui
