// ReportView: one drive's report (S3) — how fragmented it is, a map of
// where its blocks lie (S4), then its fragmented files worst first — with
// a way back to the drive list.
#pragma once

#include "model/Report.h"

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QTableView;

namespace defragr::ui {

class DriveMapView;
class Theme;

class ReportView : public QWidget {
    Q_OBJECT

public:
    explicit ReportView(const Theme &theme, QWidget *parent = nullptr);

    // Starts a report on the drive mounted there and shows it once done.
    void start(const QByteArray &mountPoint);

signals:
    void backRequested();

private:
    void showReport();
    void showFailure(const model::Message &message);
    void showFound(quint64 count, const QByteArray &latestPath);
    void showProgress(quint64 names, quint64 estimate);
    // Each second: how long it has run, and what it has found since.
    void showTick();
    void showElapsed();
    void showLatestFound();
    // Shows the run as going, or as over.
    void setRunning(bool running);

    model::Report m_report;
    QLabel *m_heading = nullptr;
    QLabel *m_figures = nullptr;
    QLabel *m_checked = nullptr;
    QLabel *m_found = nullptr;
    QLabel *m_latest = nullptr;
    QLabel *m_elapsed = nullptr;
    QProgressBar *m_progress = nullptr;
    QElapsedTimer m_clock;
    QTimer m_tick;
    // What showFound last heard, shown on the next tick: a big drive
    // finds thousands of files, faster than anyone can read them.
    quint64 m_foundCount = 0;
    QByteArray m_latestPath;
    bool m_foundShown = true;
    QWidget *m_mapKey = nullptr;
    DriveMapView *m_map = nullptr;
    QLabel *m_filesHeading = nullptr;
    QTableView *m_table = nullptr;
    QPushButton *m_back = nullptr;
};

}  // namespace defragr::ui
