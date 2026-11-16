#include "ui/MainWindow.h"

#include "BuildId.h"
#include "ui/ReportView.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace defragr::ui {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("DeFragR %1 (build %2)")
                       .arg(QLatin1String(DEFRAGR_VERSION), QLatin1String(DEFRAGR_BUILD)));

    auto *heading = new QLabel(QStringLiteral("Pick a drive, then choose what to do with it."));

    m_table = new QTableView;
    m_table->setModel(&m_drives);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_note = new QLabel;
    m_note->setWordWrap(true);

    m_report = new QPushButton(QStringLiteral("View report"));
    m_defrag = new QPushButton(QStringLiteral("Defrag now"));
    m_report->setEnabled(false);
    m_defrag->setEnabled(false);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(m_report);
    buttons->addWidget(m_defrag);

    m_drivePage = new QWidget;
    auto *layout = new QVBoxLayout(m_drivePage);
    layout->addWidget(heading);
    layout->addWidget(m_table);
    layout->addWidget(m_note);
    layout->addLayout(buttons);

    m_reportView = new ReportView;
    m_pages = new QStackedWidget;
    m_pages->addWidget(m_drivePage);
    m_pages->addWidget(m_reportView);
    setCentralWidget(m_pages);
    resize(800, 450);

    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::updateActions);
    connect(&m_drives, &model::DriveList::loaded, this, &MainWindow::updateActions);
    connect(&m_drives, &model::DriveList::failed, this, &MainWindow::showFailure);

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
        QMessageBox::information(this, QStringLiteral("Defrag now"),
                                 QStringLiteral("Defragging comes in a later version of DeFragR. "
                                                "The engine has checked this drive and will "
                                                "allow it."));
    });

    m_drives.refresh();
}

int MainWindow::selectedRow() const
{
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void MainWindow::updateActions()
{
    const int row = selectedRow();
    if (row < 0) {
        m_report->setEnabled(false);
        m_defrag->setEnabled(false);
        m_note->setText(m_drives.rowCount() == 0
                            ? QStringLiteral("DeFragR found no drives to list.")
                            : QStringLiteral("Pick a drive to see what DeFragR can do with it."));
        return;
    }
    m_report->setEnabled(m_drives.canReport(row));
    m_defrag->setEnabled(m_drives.canDefrag(row));
    const QString why = model::messages::whyNoDefrag(m_drives.drive(row));
    m_note->setText(why.isEmpty() ? QStringLiteral("DeFragR can report on this drive or defrag it.")
                                  : why);
}

void MainWindow::showFailure(const model::Message &message)
{
    updateActions();
    m_note->setText(message.action.isEmpty() ? message.what
                                             : message.what + QLatin1Char('\n') + message.action);
}

}  // namespace defragr::ui
