#include "ui/DefragView.h"

#include "model/Report.h"
#include "ui/DriveMapView.h"
#include "ui/ReadOnlyTable.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace defragr::ui {

using Stage = model::RunState::Stage;

namespace {

// The most lines the status takes at the window's usual widths, and the
// most countLines gives.
constexpr int kStatusLines = 3;
constexpr int kCountLines = 3;

// A folder on a drive, as one path.
QByteArray wholePath(const QByteArray &mountPoint, const QByteArray &below)
{
    if (below.isEmpty())
        return mountPoint;
    return mountPoint.endsWith('/') ? mountPoint + below : mountPoint + '/' + below;
}

// The widget keeps its place while hidden, so nothing below it moves.
void keepPlace(QWidget *widget)
{
    QSizePolicy policy = widget->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    widget->setSizePolicy(policy);
}

// The label has room for this many lines from the start, text at the top,
// so a line more or fewer moves nothing below it.
void keepLines(QLabel *label, int lines)
{
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // The label's own measure of that many lines, with the mark an
    // elided path carries.
    const QString was = label->text();
    label->setText(QStringList(lines, QStringLiteral("X\u2026")).join(QLatin1Char('\n')));
    label->setMinimumHeight(label->sizeHint().height());
    label->setText(was);
}

}  // namespace

DefragView::DefragView(const Theme &theme, QWidget *parent) : QWidget(parent)
{
    // Labels that show a folder's or a file's name show it as text: a name
    // is anybody's to choose, and QLabel would read some as markup.
    m_heading = new QLabel;
    m_heading->setTextFormat(Qt::PlainText);
    m_heading->setFont(theme.headingFont());
    m_back = new QPushButton(tr("Back to drives"));
    auto *top = new QHBoxLayout;
    top->addWidget(m_heading, 1);
    top->addWidget(m_back);

    m_status = new QLabel;
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_checked = new QLabel;
    m_progress = new QProgressBar;
    m_counts = new QLabel;
    m_counts->setWordWrap(true);
    m_latest = new QLabel;
    m_latest->setTextFormat(Qt::PlainText);
    // A long path must not widen the window; showCounts elides it.
    m_latest->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_elapsed = new QLabel;
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &DefragView::showTick);

    // The key sits right above the map, so a magnifier shows both.
    m_mapKey = mapKey(theme);
    m_map = new DriveMapView(theme);

    m_skippedHeading = new QLabel;
    m_skipped.setHorizontalHeaderLabels({tr("File"), tr("Why it was skipped")});
    m_skippedTable = readOnlyTable(&m_skipped);
    // The list takes the room it is given and asks for none, so it
    // cannot make that room taller. It scrolls.
    m_skippedTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);

    // DFR-0113: one room, as tall as a run's progress. While a run goes
    // it shows the progress, and afterwards the skipped files.
    m_whileRunning = new QWidget;
    auto *progress = new QVBoxLayout(m_whileRunning);
    progress->setContentsMargins(0, 0, 0, 0);
    progress->addWidget(m_checked);
    progress->addWidget(m_progress);
    progress->addWidget(m_latest);
    progress->addWidget(m_elapsed);
    m_afterwards = new QWidget;
    auto *skippedFiles = new QVBoxLayout(m_afterwards);
    skippedFiles->setContentsMargins(0, 0, 0, 0);
    skippedFiles->addWidget(m_skippedHeading);
    skippedFiles->addWidget(m_skippedTable, 1);
    m_room = new QStackedWidget;
    m_room->addWidget(m_whileRunning);
    m_room->addWidget(m_afterwards);
    m_room->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_status);
    layout->addWidget(m_counts);
    layout->addWidget(m_room);
    layout->addWidget(m_mapKey);
    layout->addWidget(m_map, 1);
    // DFR-0110: each thing above the map has its own room, shown or not,
    // so the map stays where it is through a run and after it.
    keepLines(m_status, kStatusLines);
    keepLines(m_counts, kCountLines);
    keepLines(m_latest, 1);
    // A file's name can hold glyphs taller than the window's own, which
    // would make the line taller: it stays at the height of one line.
    m_latest->setMaximumHeight(m_latest->minimumHeight());
    for (QWidget *widget : {static_cast<QWidget *>(m_checked), static_cast<QWidget *>(m_progress),
                            static_cast<QWidget *>(m_counts), static_cast<QWidget *>(m_latest),
                            static_cast<QWidget *>(m_elapsed), m_mapKey,
                            static_cast<QWidget *>(m_map)})
        keepPlace(widget);

    connect(m_back, &QPushButton::clicked, this, &DefragView::backRequested);
    connect(&m_run, &model::RunState::stageChanged, this, &DefragView::showStage);
    connect(&m_run, &model::RunState::mapProgressed, this, &DefragView::showMapProgress);
    connect(&m_run, &model::RunState::blocksMoved, m_map, &DriveMapView::showMove);
    connect(&m_run, &model::RunState::changed, m_map, &DriveMapView::followMap);
    connect(&m_run, &model::RunState::ended, this, &DefragView::showEnd);
    showStage();
}

void DefragView::setMap(const QByteArray &mountPoint, const model::DriveMap &map)
{
    m_run.setMap(mountPoint, map);
}

void DefragView::start(const QByteArray &mountPoint, const QByteArray &folder)
{
    if (m_run.isRunning())
        return;
    //: %1 is the folder's path, such as /mnt/Games/Old.
    m_heading->setText(tr("Defragging %1").arg(model::messages::path(wholePath(mountPoint, folder))));
    m_skipped.removeRows(0, m_skipped.rowCount());
    m_latest->clear();
    m_latest->setToolTip({});
    m_checked->setText(tr("Starting to read the drive."));
    // Moves back and forth until the engine's first progress arrives.
    m_progress->setRange(0, 0);
    m_map->showMap(nullptr);
    m_clock.start();
    m_run.start(mountPoint, folder);
    showCounts();
}

void DefragView::showStage()
{
    const Stage stage = m_run.stage();
    const bool running = m_run.isRunning();
    // No cancel yet (DFR-0024), so going back mid-run would leave a
    // defrag running that no screen shows.
    m_back->setEnabled(!running);
    if (running)
        m_tick.start();
    else
        m_tick.stop();

    switch (stage) {
    case Stage::Idle:
        m_status->clear();
        break;
    case Stage::ReadingMap:
        m_status->setText(tr("First DeFragR reads where everything on the drive lies, to draw "
                             "its map. On a big drive this can take a few minutes. Nothing on "
                             "the drive is changed yet. The defrag follows by itself."));
        break;
    case Stage::Defragging:
        m_status->setText(tr("Joining up the files in this folder. Each move shows on the "
                             "map: blocks leave a file's old places and land together in its "
                             "new one. You can keep using the PC."));
        // The run's own map, sweeping in; the moves then play on it.
        m_map->showMap(&m_run.map());
        // Moves back and forth until the engine has counted the work.
        m_checked->setText(tr("Counting the files in this folder that are in pieces."));
        m_progress->setRange(0, 0);
        showWork();
        break;
    case Stage::Done:
        m_status->setText(tr("Done. The map shows the drive as it is now."));
        break;
    case Stage::Failed: {
        const model::Message &problem = m_run.problem();
        m_status->setText(problem.action.isEmpty()
                              ? problem.what
                              : problem.what + QLatin1Char('\n') + problem.action);
        break;
    }
    }

    m_room->setCurrentWidget(running ? m_whileRunning : m_afterwards);
    m_checked->setVisible(running);
    m_progress->setVisible(running);
    const bool begun = stage == Stage::Defragging || stage == Stage::Done
                       || (stage == Stage::Failed && !m_run.map().isEmpty());
    m_counts->setVisible(begun);
    m_latest->setVisible(begun);
    m_elapsed->setVisible(running);
    const bool anyMap = begun && !m_run.map().isEmpty();
    m_mapKey->setVisible(anyMap);
    m_map->setVisible(anyMap);
    const bool anySkipped = !running && m_skipped.rowCount() > 0;
    m_skippedHeading->setVisible(anySkipped);
    m_skippedTable->setVisible(anySkipped);
}

void DefragView::showMapProgress(quint64 names, quint64 estimate)
{
    m_checked->setText(model::progressLine(names, estimate));
    // With no estimate the bar keeps moving back and forth.
    const int percent = model::percentChecked(names, estimate);
    if (percent >= 0) {
        m_progress->setRange(0, 100);
        m_progress->setValue(percent);
    }
}

void DefragView::showTick()
{
    //: %1 is a time, such as "2 min 5 s".
    m_elapsed->setText(tr("Running for %1").arg(model::elapsedText(m_clock.elapsed() / 1000)));
    showCounts();
}

void DefragView::showWork()
{
    if (m_run.stage() != Stage::Defragging || !m_run.work())
        return;
    m_checked->setText(model::workLine(*m_run.work()));
    m_progress->setRange(0, 100);
    m_progress->setValue(model::percentDone(*m_run.work()));
}

void DefragView::showCounts()
{
    showWork();
    m_counts->setText(model::countLines(m_run.counts()).join(QLatin1Char('\n')));
    if (m_run.latestPath().isEmpty())
        return;
    //: %1 is the path of the file DeFragR finished with most recently.
    const QString latest =
        tr("Latest: %1")
            .arg(model::messages::path(wholePath(m_run.mountPoint(), m_run.latestPath())));
    m_latest->setText(fontMetrics().elidedText(latest, Qt::ElideMiddle, m_latest->width()));
    // Qt draws a tooltip as markup when it looks like markup. Escaped
    // inside a paragraph, a file name shows as written.
    m_latest->setToolTip(QStringLiteral("<p>%1</p>").arg(latest.toHtmlEscaped()));
}

void DefragView::showEnd()
{
    // A dropped map leaves nothing the view may go on showing.
    if (m_run.map().isEmpty())
        m_map->showMap(nullptr);
    else
        m_map->followMap();

    const QList<engine::FileSkipped> &skipped = m_run.skipped();
    for (const engine::FileSkipped &file : skipped) {
        auto *path = new QStandardItem(
            model::messages::path(wholePath(m_run.mountPoint(), file.path)));
        auto *why = new QStandardItem(model::messages::whySkipped(file));
        m_skipped.appendRow({path, why});
    }
    const quint64 total = m_run.counts().skipped;
    if (total > quint64(skipped.size())) {
        // The comment sits before a whole statement: inside a conditional,
        // Qt 6.10's lupdate drops it and Qt 6.12's keeps it.
        //: %1 is how many files are listed, of more that were skipped.
        m_skippedHeading->setText(
            tr("Files DeFragR skipped, and why. Only the first %1 are listed:")
                .arg(QLocale().toString(skipped.size())));
    } else {
        m_skippedHeading->setText(tr("Files DeFragR skipped, and why:"));
    }
    showCounts();
    showStage();
}

}  // namespace defragr::ui
