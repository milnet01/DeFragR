// MainWindow: the drive list and the report-or-defrag choice (S1, S2).
// Each button is enabled from the selected drive's verdict alone, with
// the plain reason beside it when Defrag is refused (S9, S12).
#pragma once

#include "model/DriveList.h"

#include <QMainWindow>

class QLabel;
class QPushButton;
class QTableView;

namespace defragr::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    // The selected drive's row, or -1.
    int selectedRow() const;
    void updateActions();
    void showFailure(const model::Message &message);

    model::DriveList m_drives;
    QTableView *m_table = nullptr;
    QLabel *m_note = nullptr;
    QPushButton *m_report = nullptr;
    QPushButton *m_defrag = nullptr;
};

}  // namespace defragr::ui
