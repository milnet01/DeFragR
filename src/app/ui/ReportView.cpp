#include "ui/ReportView.h"

#include "ui/DriveMapView.h"
#include "ui/ReadOnlyTable.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace defragr::ui {

ReportView::ReportView(const Theme &theme, QWidget *parent) : QWidget(parent)
{
    // Labels that show a drive's or a file's name show it as text: a name
    // is anybody's to choose, and QLabel would read some as markup.
    m_heading = new QLabel;
    m_heading->setTextFormat(Qt::PlainText);
    m_heading->setFont(theme.headingFont());
    m_back = new QPushButton(tr("Back to drives"));
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
    // A long path must not widen the window; showLatestFound elides it.
    m_latest->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_elapsed = new QLabel;
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &ReportView::showTick);
    m_progress = new QProgressBar;

    // The key sits right above the map, so a magnifier shows both.
    m_mapKey = mapKey(theme);
    m_map = new DriveMapView(theme);

    m_filesHeading = new QLabel(model::filesHeading(0));

    m_table = readOnlyTable(&m_report);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_figures);
    layout->addWidget(m_checked);
    layout->addWidget(m_progress);
    layout->addWidget(m_found);
    layout->addWidget(m_latest);
    layout->addWidget(m_elapsed);
    layout->addWidget(m_mapKey);
    layout->addWidget(m_map, 1);
    layout->addWidget(m_filesHeading);
    layout->addWidget(m_table, 1);
    // Takes the space only while the table is hidden, so the text stays
    // at the top.
    layout->addStretch();

    connect(m_back, &QPushButton::clicked, this, &ReportView::backRequested);
    connect(&m_report, &model::Report::loaded, this, &ReportView::showReport);
    connect(&m_report, &model::Report::failed, this, &ReportView::showFailure);
    connect(&m_report, &model::Report::found, this, &ReportView::noteFound);
    connect(&m_report, &model::Report::progressed, this, &ReportView::showProgress);
}

void ReportView::start(const QByteArray &mountPoint)
{
    //: %1 is the drive's folder, such as /mnt/Games.
    m_heading->setText(tr("Report on %1").arg(model::messages::path(mountPoint)));
    m_figures->setText(tr("Checking every file on the drive. On a big drive this "
                          "can take a few minutes. Nothing on the drive is changed."));
    m_checked->setText(tr("Starting the check."));
    // Moves back and forth until the engine's first progress arrives.
    m_progress->setRange(0, 0);
    m_found->setText(tr("No fragmented files found yet."));
    m_latest->clear();
    m_foundShown = true;
    m_map->showMap(nullptr);
    m_clock.start();
    showElapsed();
    setRunning(true);
    m_report.run(mountPoint);
}

void ReportView::showReport()
{
    m_figures->setText(model::summaryLines(m_report.summary()).join(QLatin1Char('\n')));
    m_filesHeading->setText(model::filesHeading(m_report.unlisted()));
    m_map->showMap(&m_report.map());
    setRunning(false);
    emit loaded(m_report.mountPoint(), m_report.map());
}

void ReportView::showFailure(const model::Message &message)
{
    m_figures->setText(message.action.isEmpty()
                           ? message.what
                           : message.what + QLatin1Char('\n') + message.action);
    setRunning(false);
}

void ReportView::noteFound(quint64 count, const QByteArray &latestPath)
{
    m_foundCount = count;
    m_latestPath = latestPath;
    m_foundShown = false;
}

void ReportView::showTick()
{
    showElapsed();
    showLatestFound();
}

void ReportView::showLatestFound()
{
    if (m_foundShown)
        return;
    m_foundShown = true;
    m_found->setText(model::foundLine(m_foundCount));
    //: %1 is the path of the file found most recently.
    const QString latest = tr("Latest: %1").arg(model::messages::path(m_latestPath));
    m_latest->setText(fontMetrics().elidedText(latest, Qt::ElideMiddle, m_latest->width()));
    // Qt draws a tooltip as markup when it looks like markup. Escaped
    // inside a paragraph, a file name shows as written.
    m_latest->setToolTip(QStringLiteral("<p>%1</p>").arg(latest.toHtmlEscaped()));
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
    //: %1 is a time, such as "2 min 5 s".
    m_elapsed->setText(tr("Running for %1").arg(model::elapsedText(m_clock.elapsed() / 1000)));
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
    const bool anyMap = !running && !m_report.map().isEmpty();
    m_mapKey->setVisible(anyMap);
    m_map->setVisible(anyMap);
    const bool anyFiles = !running && m_report.rowCount() > 0;
    m_filesHeading->setVisible(anyFiles);
    m_table->setVisible(anyFiles);
}

}  // namespace defragr::ui
