// ReportView: one drive's report (S3) — how fragmented it is, then its
// fragmented files worst first — with a way back to the drive list.
#pragma once

#include "model/Report.h"

#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QTableView;

namespace defragr::ui {

class ReportView : public QWidget {
    Q_OBJECT

public:
    explicit ReportView(QWidget *parent = nullptr);

    // Starts a report on the drive mounted there and shows it once done.
    void start(const QByteArray &mountPoint);

signals:
    void backRequested();

private:
    void showReport();
    void showFailure(const model::Message &message);
    // Shows the run as going, or as over.
    void setRunning(bool running);

    model::Report m_report;
    QLabel *m_heading = nullptr;
    QLabel *m_figures = nullptr;
    QProgressBar *m_busy = nullptr;
    QLabel *m_filesHeading = nullptr;
    QTableView *m_table = nullptr;
    QPushButton *m_back = nullptr;
};

}  // namespace defragr::ui
