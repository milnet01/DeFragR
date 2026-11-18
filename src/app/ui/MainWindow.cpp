#include "ui/MainWindow.h"

#include "BuildId.h"
#include "ui/ReadOnlyTable.h"
#include "ui/ReportView.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace defragr::ui {

MainWindow::MainWindow(const Theme &theme, QWidget *parent) : QMainWindow(parent)
{
    //: %1 is the version, such as 0.1.0, and %2 the build's code.
    setWindowTitle(tr("DeFragR %1 (build %2)")
                       .arg(QLatin1String(DEFRAGR_VERSION), QLatin1String(DEFRAGR_BUILD)));

    auto *heading = new QLabel(tr("Pick a drive, then choose what to do with it."));
    heading->setFont(theme.headingFont());

    m_table = readOnlyTable(&m_drives);
    // The buttons act on one drive, so one drive is picked at a time.
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    m_note = new QLabel;
    m_note->setTextFormat(Qt::PlainText);  // it can name a filesystem, which anybody can choose
    m_note->setWordWrap(true);

    m_refresh = new QPushButton(model::messages::refreshButton());
    m_report = new QPushButton(tr("View report"));
    m_defrag = new QPushButton(tr("Defrag now"));
    m_report->setEnabled(false);
    m_defrag->setEnabled(false);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_refresh);
    buttons->addStretch();
    buttons->addWidget(m_report);
    buttons->addWidget(m_defrag);

    m_drivePage = new QWidget;
    auto *layout = new QVBoxLayout(m_drivePage);
    layout->addWidget(heading);
    layout->addWidget(m_table);
    layout->addWidget(m_note);
    layout->addLayout(buttons);

    m_reportView = new ReportView(theme);
    m_pages = new QStackedWidget;
    m_pages->addWidget(m_drivePage);
    m_pages->addWidget(m_reportView);
    setCentralWidget(m_pages);
    resize(Theme::kWindowStartSize);

    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::updateActions);
    connect(&m_drives, &model::DriveList::loaded, this, &MainWindow::updateActions);
    connect(&m_drives, &model::DriveList::failed, this, &MainWindow::showFailure);
    connect(m_refresh, &QPushButton::clicked, this, &MainWindow::listDrives);

    connect(m_report, &QPushButton::clicked, this, [this] {
        const int row = selectedRow();
        if (row < 0)
            return;
        m_reportView->start(m_drives.drive(row).mountPoint);
        m_pages->setCurrentWidget(m_reportView);
    });
    connect(m_reportView, &ReportView::backRequested, this,
            [this] { m_pages->setCurrentWidget(m_drivePage); });

    // Defragging is not built yet; it comes with 0.3.0. The button still
    // follows the verdict.
    connect(m_defrag, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Defrag now"),
                                 model::messages::defragComesLater());
    });

    listDrives();
}

void MainWindow::listDrives()
{
    m_drives.refresh();
    updateActions();
}

int MainWindow::selectedRow() const
{
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void MainWindow::updateActions()
{
    m_refresh->setEnabled(!m_drives.isListing());
    const int row = selectedRow();
    if (m_drives.isListing()) {
        m_report->setEnabled(false);
        m_defrag->setEnabled(false);
        m_note->setText(tr("Listing drives…"));
        return;
    }
    if (row < 0) {
        m_report->setEnabled(false);
        m_defrag->setEnabled(false);
        m_note->setText(m_drives.rowCount() == 0
                            ? model::messages::noDrives()
                            : tr("Pick a drive to see what DeFragR can do with it."));
        return;
    }
    m_report->setEnabled(m_drives.canReport(row));
    m_defrag->setEnabled(m_drives.canDefrag(row));
    const QString why = model::messages::whyNoDefrag(m_drives.drive(row));
    m_note->setText(why.isEmpty() ? model::messages::defragAllowed() : why);
}

void MainWindow::showFailure(const model::Message &message)
{
    updateActions();
    m_note->setText(message.action.isEmpty() ? message.what
                                             : message.what + QLatin1Char('\n') + message.action);
}

}  // namespace defragr::ui
