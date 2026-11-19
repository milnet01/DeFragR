// MainWindow: the drive list and the report-or-defrag choice (S1, S2).
// Each button is enabled from the selected drive's verdict alone, with
// the plain reason beside it when Defrag is refused (S9, S12). A report,
// or a defrag of one folder, takes the drive list's place until the user
// goes back. A defrag starts only once a folder on the drive is picked
// and the question about it is answered yes
// (docs/specs/DFR-0018-window-folder-defrag.md § 4.4).
#pragma once

#include "model/DriveList.h"

#include <QMainWindow>

#include <functional>
#include <utility>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTableView;

namespace defragr::ui {

class DefragView;
class ReportView;
class Theme;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const Theme &theme, QWidget *parent = nullptr);

    // Asks for the folder to defrag, starting at `start`; gives its path,
    // or nothing when the user backs out. The desktop's folder dialog
    // unless a test sets its own.
    using FolderPicker = std::function<QString(QWidget *parent, const QString &start)>;
    void setFolderPicker(FolderPicker picker) { m_pickFolder = std::move(picker); }

private:
    // The selected drive's row, or -1.
    int selectedRow() const;
    void listDrives();
    void updateActions();
    void showFailure(const model::Message &message);
    // "Defrag now": the folder, the question, then the run.
    void defragAFolder();

    model::DriveList m_drives;
    QStackedWidget *m_pages = nullptr;
    QWidget *m_drivePage = nullptr;
    ReportView *m_reportView = nullptr;
    DefragView *m_defragView = nullptr;
    FolderPicker m_pickFolder;
    QTableView *m_table = nullptr;
    QLabel *m_note = nullptr;
    QPushButton *m_refresh = nullptr;
    QPushButton *m_report = nullptr;
    QPushButton *m_defrag = nullptr;
};

}  // namespace defragr::ui
