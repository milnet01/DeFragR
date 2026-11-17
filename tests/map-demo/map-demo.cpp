// The map demo: a window showing a recorded report's drive map, with
// cells changing by script, so the map's motion can be watched and judged
// before the engine can defrag (DFR-0014). Not a test: nothing checks it.
//
//   defragr-map-demo [RECORDING]
//
// RECORDING is an engine report saved as tests/fixtures/README.md says;
// by default the image test's. Every so often a few fragmented cells
// turn whole. Once none are left, the map sweeps in again from the start.
#include "ui/DriveMapView.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>
#include <cstdlib>

using namespace defragr;

namespace {

model::DriveMap readMap(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "defragr-map-demo: cannot read %s\n", qPrintable(path));
        std::exit(2);
    }
    engine::RecordDecoder decoder;
    decoder.feed(file.readAll());
    decoder.finish();
    model::DriveMap map;
    for (const engine::Record &record : decoder.takeRecords())
        if (const auto *runs = std::get_if<engine::MapRuns>(&record))
            map.add(*runs);
    return map;
}

// Turns up to four neighbouring fragmented cells whole; false when none
// are left.
bool defragSome(ui::DriveMapView &view)
{
    QList<model::CellState> cells = view.cells();
    QList<qsizetype> fragmented;
    for (qsizetype i = 0; i < cells.size(); ++i)
        if (cells[i] == model::CellState::Fragmented)
            fragmented.append(i);
    if (fragmented.isEmpty())
        return false;
    qsizetype at = fragmented[QRandomGenerator::global()->bounded(fragmented.size())];
    for (int n = 0; n < 4 && at < cells.size() && cells[at] == model::CellState::Fragmented; ++n)
        cells[at++] = model::CellState::Whole;
    view.changeCells(cells);
    return true;
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString recording = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                       : QStringLiteral(DEFRAGR_DEMO_RECORDING);
    const model::DriveMap map = readMap(recording);
    if (map.isEmpty()) {
        std::fprintf(stderr, "defragr-map-demo: %s has no map runs\n", qPrintable(recording));
        return 2;
    }

    const ui::Theme theme(ui::Theme::Id::Desktop, QGuiApplication::palette(),
                          QGuiApplication::font());
    theme.apply();
    QWidget window;
    window.setWindowTitle(QStringLiteral("DeFragR map demo"));
    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(ui::mapKey(theme));
    auto *view = new ui::DriveMapView(theme);
    layout->addWidget(view, 1);
    window.resize(view->sizeHint() + QSize(40, 80));
    window.show();
    view->showMap(&map);

    QTimer script;
    script.setInterval(700);
    QObject::connect(&script, &QTimer::timeout, [&] {
        if (!defragSome(*view))
            view->showMap(&map);
    });
    // Starts once the first sweep is over.
    QTimer::singleShot(ui::DriveMapView::kSweepMs + ui::DriveMapView::kEaseMs, &script,
                       qOverload<>(&QTimer::start));
    return app.exec();
}
