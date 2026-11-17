#include "ui/ReportView.h"

#include <QFile>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace defragr::ui {

ReportView::ReportView(QWidget *parent) : QWidget(parent)
{
    // Labels that show a drive's or a file's name show it as text: a name
    // is anybody's to choose, and QLabel would read some as markup.
    m_heading = new QLabel;
    m_heading->setTextFormat(Qt::PlainText);
    m_back = new QPushButton(QStringLiteral("Back to drives"));
    auto *top = new QHBoxLayout;
    top->addWidget(m_heading, 1);
    top->addWidget(m_back);

    m_figures = new QLabel;
    m_figures->setWordWrap(true);

    // While it runs: how far it has got, what it has found, and for how
    // long.
    m_checked = new QLabel;
    m_found = new QLabel;
    m_latest = new QLabel;
    m_latest->setTextFormat(Qt::PlainText);
    // A long path must not widen the window; showFound elides it.
    m_latest->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_elapsed = new QLabel;
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &ReportView::showElapsed);
    m_progress = new QProgressBar;

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
    layout->addWidget(m_checked);
    layout->addWidget(m_progress);
    layout->addWidget(m_found);
    layout->addWidget(m_latest);
    layout->addWidget(m_elapsed);
    layout->addWidget(m_filesHeading);
    layout->addWidget(m_table, 1);
    // Takes the space only while the table is hidden, so the text stays
    // at the top.
    layout->addStretch();

    connect(m_back, &QPushButton::clicked, this, &ReportView::backRequested);
    connect(&m_report, &model::Report::loaded, this, &ReportView::showReport);
    connect(&m_report, &model::Report::failed, this, &ReportView::showFailure);
    connect(&m_report, &model::Report::found, this, &ReportView::showFound);
    connect(&m_report, &model::Report::progressed, this, &ReportView::showProgress);
}

void ReportView::start(const QByteArray &mountPoint)
{
    m_heading->setText(QStringLiteral("Report on %1").arg(QFile::decodeName(mountPoint)));
    m_figures->setText(QStringLiteral("Checking every file on the drive. On a big drive this "
                                      "can take a few minutes. Nothing on the drive is changed."));
    m_checked->setText(QStringLiteral("Starting the check."));
    // Moves back and forth until the engine's first progress arrives.
    m_progress->setRange(0, 0);
    m_found->setText(QStringLiteral("No fragmented files found yet."));
    m_latest->clear();
    m_clock.start();
    showElapsed();
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

void ReportView::showFound(quint64 count, const QByteArray &latestPath)
{
    m_found->setText(QStringLiteral("Fragmented files found so far: %1")
                         .arg(QLocale().toString(count)));
    const QString latest = QStringLiteral("Latest: %1").arg(QFile::decodeName(latestPath));
    m_latest->setText(fontMetrics().elidedText(latest, Qt::ElideMiddle, m_latest->width()));
    m_latest->setToolTip(latest);
}

void ReportView::showProgress(quint64 names, quint64 estimate)
{
    m_checked->setText(model::progressLine(names, estimate));
    // With no estimate the bar keeps moving back and forth.
    const int percent = model::percentChecked(names, estimate);
    if (percent >= 0) {
        m_progress->setRange(0, 100);
        m_progress->setValue(percent);
    }
}

void ReportView::showElapsed()
{
    const qint64 seconds = m_clock.elapsed() / 1000;
    QString text;
    if (seconds >= 3600)
        text = QStringLiteral("%1 h %2 min").arg(seconds / 3600).arg(seconds % 3600 / 60);
    else if (seconds >= 60)
        text = QStringLiteral("%1 min %2 s").arg(seconds / 60).arg(seconds % 60);
    else
        text = QStringLiteral("%1 s").arg(seconds);
    m_elapsed->setText(QStringLiteral("Running for %1").arg(text));
}

void ReportView::setRunning(bool running)
{
    // No cancel yet (DFR-0024), so going back mid-run would leave a
    // report running that no screen shows.
    m_back->setEnabled(!running);
    m_checked->setVisible(running);
    m_progress->setVisible(running);
    m_found->setVisible(running);
    m_latest->setVisible(running);
    m_elapsed->setVisible(running);
    if (running)
        m_tick.start();
    else
        m_tick.stop();
    const bool anyFiles = !running && m_report.rowCount() > 0;
    m_filesHeading->setVisible(anyFiles);
    m_table->setVisible(anyFiles);
}

}  // namespace defragr::ui
