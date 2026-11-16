#include "ui/ReportView.h"

#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace defragr::ui {

ReportView::ReportView(QWidget *parent) : QWidget(parent)
{
    m_heading = new QLabel;
    m_back = new QPushButton(QStringLiteral("Back to drives"));
    auto *top = new QHBoxLayout;
    top->addWidget(m_heading, 1);
    top->addWidget(m_back);

    m_figures = new QLabel;
    m_figures->setWordWrap(true);

    // The engine sends no progress during a report, so the bar only
    // shows that the run is going.
    m_busy = new QProgressBar;
    m_busy->setRange(0, 0);

    m_filesHeading = new QLabel(QStringLiteral("Fragmented files, worst first:"));

    m_table = new QTableView;
    m_table->setModel(&m_report);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_figures);
    layout->addWidget(m_busy);
    layout->addWidget(m_filesHeading);
    layout->addWidget(m_table, 1);

    connect(m_back, &QPushButton::clicked, this, &ReportView::backRequested);
    connect(&m_report, &model::Report::loaded, this, &ReportView::showReport);
    connect(&m_report, &model::Report::failed, this, &ReportView::showFailure);
}

void ReportView::start(const QByteArray &mountPoint)
{
    m_heading->setText(QStringLiteral("Report on %1").arg(QFile::decodeName(mountPoint)));
    m_figures->setText(QStringLiteral("Checking every file on the drive. On a big drive this "
                                      "can take a few minutes. Nothing on the drive is changed."));
    setRunning(true);
    m_report.run(mountPoint);
}

void ReportView::showReport()
{
    m_figures->setText(model::summaryLines(m_report.summary()).join(QLatin1Char('\n')));
    setRunning(false);
}

void ReportView::showFailure(const model::Message &message)
{
    m_figures->setText(message.action.isEmpty()
                           ? message.what
                           : message.what + QLatin1Char('\n') + message.action);
    setRunning(false);
}

void ReportView::setRunning(bool running)
{
    // No cancel yet (DFR-0024), so going back mid-run would leave a
    // report running that no screen shows.
    m_back->setEnabled(!running);
    m_busy->setVisible(running);
    const bool anyFiles = !running && m_report.rowCount() > 0;
    m_filesHeading->setVisible(anyFiles);
    m_table->setVisible(anyFiles);
}

}  // namespace defragr::ui
