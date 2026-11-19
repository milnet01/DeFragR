// The map demo: a window showing a recorded report's drive map, with a
// made-up defrag playing on it, so the map's motion can be watched and
// judged without root or a drive to defrag (DFR-0014, DFR-0018). Not a
// test: nothing checks it.
//
//   defragr-map-demo [RECORDING]
//
// RECORDING is an engine report saved as tests/fixtures/README.md says;
// by default the image test's. Several times a second a piece of a
// fragmented file moves into free space, as the engine's BLOCKS_MOVED
// would say, a few pieces to a file. Once none are left, the map sweeps
// in again from the start.
#include "ui/DriveMapView.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace defragr;

namespace {

// What the demo needs of a recording: the map, and where its fragmented
// files and its free space lie.
struct Recording {
    model::DriveMap map;
    QList<engine::MapRun> fragmented;
    QList<engine::MapRun> free;
};

Recording readRecording(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "defragr-map-demo: cannot read %s\n", qPrintable(path));
        std::exit(2);
    }
    engine::RecordDecoder decoder;
    decoder.feed(file.readAll());
    decoder.finish();
    Recording recording;
    for (const engine::Record &record : decoder.takeRecords())
        if (const auto *runs = std::get_if<engine::MapRuns>(&record)) {
            recording.map.add(*runs);
            if (runs->kind == engine::MapFragmentedFile)
                recording.fragmented += runs->runs;
            else if (runs->kind == engine::MapFree)
                recording.free += runs->runs;
        }
    return recording;
}

// The made-up defrag: each step moves the next piece of a fragmented file
// to the front of the free space left.
class Defrag {
public:
    Defrag(const Recording &recording, ui::DriveMapView &view)
        : m_recording(recording), m_view(view)
    {
        restart();
    }

    void restart()
    {
        m_map = m_recording.map;
        m_fragmented = m_recording.fragmented;
        m_free = m_recording.free;
        m_pieces = 0;
        m_view.showMap(&m_map);
    }

    // False when no fragmented piece, or no free space, is left.
    bool step()
    {
        if (m_fragmented.isEmpty() || m_free.isEmpty())
            return false;
        engine::MapRun &from = m_fragmented.first();
        engine::MapRun &to = m_free.first();
        const quint64 blocks = std::min({from.length, to.length, kChunkBlocks});
        const engine::BlocksMoved move{{{from.start, blocks}}, {{to.start, blocks}}};
        from.start += blocks;
        from.length -= blocks;
        to.start += blocks;
        to.length -= blocks;
        if (from.length == 0)
            m_fragmented.removeFirst();
        if (to.length == 0)
            m_free.removeFirst();
        m_map.move(move);
        m_view.showMove(move);
        // A file's turn ends every few pieces, in one piece.
        if (++m_pieces % 4 == 0) {
            m_map.endTurn(model::CellState::Whole);
            m_view.followMap();
        }
        return true;
    }

private:
    // The engine moves at most this many blocks at once (move.asm).
    static constexpr quint64 kChunkBlocks = 1024;

    const Recording &m_recording;
    ui::DriveMapView &m_view;
    model::DriveMap m_map;
    QList<engine::MapRun> m_fragmented;
    QList<engine::MapRun> m_free;
    int m_pieces = 0;
};

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString recording = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                       : QStringLiteral(DEFRAGR_DEMO_RECORDING);
    const Recording saved = readRecording(recording);
    if (saved.map.isEmpty()) {
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
    Defrag defrag(saved, *view);

    QTimer script;
    script.setInterval(120);
    QObject::connect(&script, &QTimer::timeout, [&] {
        if (!defrag.step())
            defrag.restart();
    });
    // Starts once the first sweep is over.
    QTimer::singleShot(ui::DriveMapView::kSweepMs + ui::DriveMapView::kArriveMs, &script,
                       qOverload<>(&QTimer::start));
    return app.exec();
}
