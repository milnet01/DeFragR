// DriveMapView: the map painted with the theme's patterns (S4, S10). A
// cell never jumps: it fades from old to new while it glows past its
// edges, then settles; a new map fills in first block to last, and the
// frame timer stops once nothing moves. During a defrag each move plays
// as a journey (docs/specs/DFR-0018-window-folder-defrag.md INV-6 to
// INV-10). The view reads a clock this test sets, so every frame is a
// chosen moment.
#include "ui/DriveMapView.h"
#include "ui/Theme.h"

#include <QImage>
#include <QPaintEvent>
#include <QPainter>
#include <QScreen>
#include <QTest>

using namespace defragr::engine;
using defragr::model::CellState;
using defragr::model::DriveMap;
using defragr::ui::DriveMapView;
using defragr::ui::Theme;

namespace {

constexpr int kColumns = 10;
constexpr int kRows = 4;

// One block per cell: a row each of whole, fragmented, filesystem and
// free.
DriveMap rowsMap()
{
    DriveMap map;
    map.add({MapWholeFile, {{0, 10}}});
    map.add({MapFragmentedFile, {{10, 10}}});
    map.add({MapFilesystem, {{20, 10}}});
    map.add({MapFree, {{30, 10}}});
    return map;
}

CellState rowState(int index)
{
    constexpr CellState rows[] = {CellState::Whole, CellState::Fragmented,
                                  CellState::Filesystem, CellState::Free};
    return rows[index / kColumns];
}

QImage cellImage(const Theme &theme, CellState state)
{
    const int side = theme.cellSide();
    QImage image(side, side, QImage::Format_ARGB32);
    QPainter painter(&image);
    theme.paintCell(painter, QRect(0, 0, side, side), state);
    return image;
}

QImage blankImage(const Theme &theme)
{
    QImage image(theme.cellSide(), theme.cellSide(), QImage::Format_ARGB32);
    image.fill(theme.mapBackground());
    return image;
}

bool samePixels(const QImage &shot, QPoint at, const QImage &expected)
{
    for (int y = 0; y < expected.height(); ++y)
        for (int x = 0; x < expected.width(); ++x)
            if (shot.pixelColor(at + QPoint(x, y)).rgb() != expected.pixelColor(x, y).rgb())
                return false;
    return true;
}

// The area of each paint a widget is asked for.
class PaintWatcher : public QObject {
public:
    QList<qint64> areas;

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Paint) {
            qint64 area = 0;
            for (const QRect &rect : static_cast<QPaintEvent *>(event)->region())
                area += qint64(rect.width()) * rect.height();
            areas.append(area);
        }
        return false;
    }
};

}  // namespace

class TestDriveMapView : public QObject {
    Q_OBJECT

    Theme m_theme{Theme::Id::DarkHighContrast, QPalette(QColor("#31363b"), QColor("#202326")),
                  QFont(QStringLiteral("Sans"), 10)};
    DriveMap m_map = rowsMap();
    qint64 m_now = 0;

    int pitch() const { return m_theme.cellSide() + m_theme.cellGap(); }
    QPoint cellAt(int index) const
    {
        return {index % kColumns * pitch(), index / kColumns * pitch()};
    }

    // A view of exactly kColumns by kRows cells, on this test's clock.
    std::unique_ptr<DriveMapView> makeView()
    {
        auto view = std::make_unique<DriveMapView>(m_theme);
        view->setClock([this] { return m_now; });
        view->setMotion(true);  // whatever this desktop's setting
        view->resize(kColumns * pitch() - m_theme.cellGap(), kRows * pitch() - m_theme.cellGap());
        view->show();
        return view;
    }

    // A view showing the map, its sweep over.
    std::unique_ptr<DriveMapView> settledView()
    {
        auto view = makeView();
        view->showMap(&m_map);
        m_now += DriveMapView::kSweepMs + DriveMapView::kArriveMs;
        return view;
    }

private slots:
    void init() { m_now = 0; }

    void settledCellsAreTheThemesPatterns()
    {
        const auto view = settledView();
        QCOMPARE(view->capacity(), kColumns * kRows);
        const QImage shot = view->grab().toImage();
        for (int i = 0; i < kColumns * kRows; ++i)
            QVERIFY2(samePixels(shot, cellAt(i), cellImage(m_theme, rowState(i))),
                     qPrintable(QString::number(i)));
    }

    // A quarter through the sweep, the first cell is in and the last is
    // still blank. Three quarters through, the band's glow has moved on
    // and the first cell shows just its pattern.
    void sweepGoesFirstToLast()
    {
        const auto view = makeView();
        view->showMap(&m_map);
        QVERIFY(samePixels(view->grab().toImage(), cellAt(0), blankImage(m_theme)));
        m_now = DriveMapView::kSweepMs / 4;
        QImage shot = view->grab().toImage();
        QVERIFY(!samePixels(shot, cellAt(0), blankImage(m_theme)));
        QVERIFY(samePixels(shot, cellAt(kColumns * kRows - 1), blankImage(m_theme)));
        m_now = DriveMapView::kSweepMs * 3 / 4;
        shot = view->grab().toImage();
        QVERIFY(samePixels(shot, cellAt(0), cellImage(m_theme, CellState::Whole)));
    }

    // A changed cell shows its old pattern, then something between, then
    // its new one; once settled its neighbours are as they were.
    void changeFadesRatherThanJumps()
    {
        const auto view = settledView();
        QList<CellState> cells = view->cells();
        cells[0] = CellState::Fragmented;
        view->changeCells(cells);
        QVERIFY(view->isMoving());
        const QImage before = cellImage(m_theme, CellState::Whole);
        const QImage after = cellImage(m_theme, CellState::Fragmented);

        QImage shot = view->grab().toImage();
        QVERIFY(samePixels(shot, cellAt(0), before));
        m_now += DriveMapView::kChangeMs / 2;
        shot = view->grab().toImage();
        QVERIFY(!samePixels(shot, cellAt(0), before));
        QVERIFY(!samePixels(shot, cellAt(0), after));
        m_now += DriveMapView::kChangeMs / 2;
        shot = view->grab().toImage();
        QVERIFY(samePixels(shot, cellAt(0), after));
        QVERIFY(samePixels(shot, cellAt(1), before));
        QCOMPARE(view->cells(), cells);
    }

    // So a change catches the eye, the changed cell lights up brighter
    // than either of its patterns and its glow reaches past its edges,
    // over the cell beside it and the one below.
    void changeGlowsPastItsEdges()
    {
        const auto view = settledView();
        QList<CellState> cells = view->cells();
        const int changed = kColumns + 4;  // a fragmented cell, mid row
        cells[changed] = CellState::Whole;
        view->changeCells(cells);
        const QImage still = view->grab().toImage();
        m_now += DriveMapView::kChangeMs / 5;
        const QImage lit = view->grab().toImage();

        const auto brightness = [&](const QImage &shot, int index) {
            qint64 sum = 0;
            for (int y = 0; y < m_theme.cellSide(); ++y)
                for (int x = 0; x < m_theme.cellSide(); ++x)
                    sum += qGray(shot.pixel(cellAt(index) + QPoint(x, y)));
            return sum;
        };
        const qint64 whole = [&] {
            qint64 sum = 0;
            const QImage image = cellImage(m_theme, CellState::Whole);
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    sum += qGray(image.pixel(x, y));
            return sum;
        }();
        QCOMPARE_GT(brightness(lit, changed), whole);
        QCOMPARE_GT(brightness(lit, changed), brightness(still, changed));
        for (const int neighbour : {changed + 1, changed + kColumns})
            QCOMPARE_GT(brightness(lit, neighbour), brightness(still, neighbour));
    }

    // The frame timer runs only while something moves.
    void frameTimerStopsWhenStill()
    {
        const auto view = makeView();
        view->showMap(&m_map);
        QVERIFY(view->isMoving());
        m_now += DriveMapView::kSweepMs + DriveMapView::kArriveMs;
        QTRY_VERIFY(!view->isMoving());
        // Asking for what is already shown moves nothing.
        view->changeCells(view->cells());
        QVERIFY(!view->isMoving());
    }

    // In a right-to-left language the first block is top right. Here the
    // first block alone is fragmented, so it cannot be mistaken for the
    // rest of its row.
    void rightToLeftStartsTopRight()
    {
        DriveMap map;
        map.add({MapFragmentedFile, {{0, 1}}});
        map.add({MapWholeFile, {{1, 9}}});
        map.add({MapFragmentedFile, {{10, 10}}});
        map.add({MapFilesystem, {{20, 10}}});
        map.add({MapFree, {{30, 10}}});
        const auto view = makeView();
        view->setLayoutDirection(Qt::RightToLeft);
        view->showMap(&map);
        m_now += DriveMapView::kSweepMs + DriveMapView::kArriveMs;
        const QImage shot = view->grab().toImage();
        const QPoint topRight(view->width() - m_theme.cellSide(), 0);
        QVERIFY(samePixels(shot, topRight, cellImage(m_theme, CellState::Fragmented)));
        QVERIFY(samePixels(shot, QPoint(0, 0), cellImage(m_theme, CellState::Whole)));
        QVERIFY(samePixels(shot, QPoint(0, pitch()), cellImage(m_theme, CellState::Fragmented)));
    }

    // A smaller view still shows the whole drive, in fewer cells, at once.
    void resizeShowsTheWholeDrive()
    {
        const auto view = settledView();
        view->resize(kColumns / 2 * pitch() - m_theme.cellGap(), view->height());
        QCOMPARE(view->capacity(), kColumns * kRows / 2);
        QList<CellState> expected;
        for (const CellState state : {CellState::Whole, CellState::Fragmented,
                                      CellState::Filesystem, CellState::Free})
            expected.append(QList<CellState>(kColumns / 2, state));
        QCOMPARE(view->cells(), expected);
        // No second sweep: the last cell is in already.
        const QImage shot = view->grab().toImage();
        QVERIFY(samePixels(shot, QPoint(4 * pitch(), 3 * pitch()),
                           cellImage(m_theme, CellState::Free)));
    }

    // S10, WCAG 2.2 SC 2.3.1: however often a cell is asked to change, it
    // starts a change, and so glows, at most three times a second. Once
    // still, it shows the last state asked for.
    void changesAtMostThreeTimesASecond()
    {
        const auto view = settledView();
        QList<CellState> cells = view->cells();
        CellState shown = cells[0];
        int starts = 0;
        for (int step = 0; step < 100; ++step) {  // every 10 ms for a second
            if (step % 10 == 0) {                 // a change every 100 ms
                cells[0] = step % 20 == 0 ? CellState::Fragmented : CellState::Whole;
                view->changeCells(cells);
            }
            if (view->cells()[0] != shown) {
                shown = view->cells()[0];
                ++starts;
            }
            m_now += 10;
        }
        QVERIFY2(starts <= 3, qPrintable(QString::number(starts)));
        m_now += 1000;
        QCOMPARE(view->cells()[0], cells[0]);
    }

    // A resize mid-sweep shows every cell settled: re-sorting into new
    // cells starts no glow, so dragging the window edge cannot flash.
    void resizeMidSweepStartsNoGlow()
    {
        const auto view = makeView();
        view->showMap(&m_map);
        m_now = DriveMapView::kSweepMs / 2;
        // Twice as tall, so the map is sorted into twice the cells.
        view->resize(view->width(), 2 * kRows * pitch() - m_theme.cellGap());
        const QImage shot = view->grab().toImage();
        const auto still = makeView();
        still->resize(view->size());
        still->showMap(&m_map);
        m_now += DriveMapView::kSweepMs + DriveMapView::kArriveMs;
        QCOMPARE(shot, still->grab().toImage());
    }

    // A view of `map` at one block a cell, its sweep over.
    std::unique_ptr<DriveMapView> settledViewOf(const DriveMap &map, int rows = kRows)
    {
        auto view = makeView();
        view->resize(view->width(), rows * pitch() - m_theme.cellGap());
        view->showMap(&map);
        m_now += DriveMapView::kSweepMs + DriveMapView::kArriveMs;
        return view;
    }

    // Lets time pass on the view's clock until nothing moves, a launch
    // gap at a time.
    void runUntilStill(DriveMapView &view)
    {
        for (int step = 0; step < 100 && view.isMoving(); ++step) {
            m_now += DriveMapView::kLaunchGapMs;
            QTest::qWait(20);  // the frame timer's own time
        }
        QVERIFY(!view.isMoving());
    }

    // DFR-0018 INV-6: however the moves arrive, the cells end as the map.
    // Blocks land where an earlier move's are still on their way, a file
    // ends in pieces, and blocks that have not landed yet move again, so
    // a cell due to change must be left as it is after all.
    void movesEndAsTheMap()
    {
        DriveMap map = rowsMap();
        const auto view = settledViewOf(map);
        const auto move = [&](const BlocksMoved &blocks) {
            map.move(blocks);
            view->showMove(blocks);
        };
        move({{{10, 2}}, {{30, 2}}});
        m_now += 50;
        move({{{12, 3}}, {{32, 3}}});
        m_now += 20;
        map.endTurn(CellState::Fragmented);
        view->followMap();
        m_now += 30;
        // Before the first blocks have landed, they move on.
        move({{{30, 2}}, {{36, 2}}});
        map.endTurn(CellState::Whole);
        view->followMap();
        QVERIFY(view->isMoving());
        QVERIFY(view->cells() != map.cells(view->capacity()));
        runUntilStill(*view);
        QCOMPARE(view->cells(), map.cells(view->capacity()));
        QCOMPARE(view->cells()[30], CellState::Free);
        QCOMPARE(view->cells()[33], CellState::Fragmented);
        QCOMPARE(view->cells()[36], CellState::Whole);
        QCOMPARE(view->motesInFlight(), 0);
    }

    // DFR-0018 INV-7: a move waits for the next launch, flies, and lands,
    // all within the launch gap, the flight and the landing spread. The
    // second move here must wait for its launch; it is not held up behind
    // the first move's flight.
    void movesShowWithinASecond()
    {
        DriveMap map = rowsMap();
        const auto view = settledViewOf(map);
        const BlocksMoved first{{{10, 1}}, {{30, 1}}};
        map.move(first);
        view->showMove(first);  // launches at once: nothing went before
        QVERIFY(view->motesInFlight() >= 3);
        QCOMPARE(view->cells()[30], CellState::Free);  // not landed yet

        m_now += 10;
        const qint64 shown = m_now;
        const BlocksMoved second{{{12, 2}}, {{33, 2}}};
        map.move(second);
        view->showMove(second);
        const qint64 bound = shown + DriveMapView::kLaunchGapMs + DriveMapView::kFlightMs
                             + DriveMapView::kLandSpreadMs;
        // Its launch comes once the gap has passed.
        m_now = shown + DriveMapView::kLaunchGapMs;
        view->followMap();
        QCOMPARE(view->cells()[33], CellState::Free);
        m_now = bound;
        const QList<CellState> cells = view->cells();
        QCOMPARE(cells[12], CellState::Free);
        QCOMPARE(cells[13], CellState::Free);
        QCOMPARE(cells[33], CellState::Whole);
        QCOMPARE(cells[34], CellState::Whole);
        QCOMPARE(cells[30], CellState::Whole);
        QCOMPARE(view->markerCell(), 34);
        QCOMPARE(view->sourceCell(), 13);
    }

    // DFR-0018 INV-8, S10: blocks moved onto one cell and off it again
    // every 100 ms start at most three changes there in a second. With
    // motion off nothing but the view's own rule delays a change, so the
    // rule is all that is tested. With motion on, the same moves never
    // land: each launch finds the cell due back where it is, and nothing
    // glows.
    void movesFlashNoMoreThanThreeASecond()
    {
        for (const bool motion : {false, true}) {
            DriveMap map = rowsMap();
            const auto view = settledViewOf(map);
            view->setMotion(motion);
            CellState shown = view->cells()[30];
            int starts = 0;
            for (int step = 0; step < 100; ++step) {  // every 10 ms for a second
                if (step % 10 == 0) {
                    const bool onto = step % 20 == 0;
                    const BlocksMoved blocks = onto ? BlocksMoved{{{10, 1}}, {{30, 1}}}
                                                    : BlocksMoved{{{30, 1}}, {{10, 1}}};
                    map.move(blocks);
                    view->showMove(blocks);
                }
                view->followMap();  // a launch, when one is due
                if (view->cells()[30] != shown) {
                    shown = view->cells()[30];
                    ++starts;
                }
                m_now += 10;
            }
            if (motion)
                QCOMPARE(starts, 0);
            else
                QVERIFY2(starts >= 2 && starts <= 3, qPrintable(QString::number(starts)));
            m_now += 2000;
            view->followMap();
            m_now += 2000;
            QCOMPARE(view->cells(), map.cells(view->capacity()));
        }
    }

    // DFR-0018 INV-9: frames repaint only what moved, and leave nothing
    // stale. The screen's own pixels are read, since grab() paints the
    // whole view afresh and would hide a row a frame forgot.
    void onlyWhatMovesIsRepainted()
    {
        // Rows enough that the marker and its pulse are a small part.
        const int rows = 20;
        DriveMap map;
        map.add({MapFragmentedFile, {{10, 10}}});
        map.add({MapFree, {{20, 180}}});
        map.add({MapWholeFile, {{0, 10}}});
        const auto view = settledViewOf(map, rows);
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
        QTest::qWait(50);
        QCOMPARE(view->capacity(), kColumns * rows);

        const auto screen = [&] { return view->screen()->grabWindow(view->winId()).toImage(); };
        // How many of the cells from `first` to `last` are not their
        // settled pattern on screen.
        const auto unsettled = [&](const QImage &shot, int first, int last) {
            const QList<CellState> cells = map.cells(view->capacity());
            int count = 0;
            for (int i = first; i <= last; ++i)
                count += !samePixels(shot, cellAt(i), cellImage(m_theme, cells[i]));
            return count;
        };

        PaintWatcher paints;
        view->installEventFilter(&paints);
        // From the second row to the ninth: the small blocks cross rows
        // where no cell changes.
        const BlocksMoved blocks{{{12, 3}}, {{84, 3}}};
        map.move(blocks);
        view->showMove(blocks);
        QVERIFY(view->isMoving());
        // Long steps, so a cell or a small block is well short of done at
        // one frame and done by the next: the frame that must clear it.
        m_now += DriveMapView::kFlightMs / 2;
        QTest::qWait(50);
        QVERIFY(view->motesInFlight() > 0);
        QVERIFY2(unsettled(screen(), 3 * kColumns, 7 * kColumns - 1) > 0,
                 "no small block shows between the rows that change");
        for (int step = 0; step < 40 && view->isMoving(); ++step) {
            m_now += 300;
            QTest::qWait(50);
        }
        QVERIFY(!view->isMoving());
        QTest::qWait(50);

        // Three rows changed or glowed at each end, and the small blocks
        // crossed between. No frame came near the whole view.
        const qint64 whole = qint64(view->width()) * view->height();
        QVERIFY(paints.areas.size() >= 4);
        for (const qint64 area : paints.areas)
            QVERIFY2(area <= whole * 2 / 3, qPrintable(QString::number(area)));

        QCOMPARE(view->cells(), map.cells(view->capacity()));
        QCOMPARE(view->markerCell(), 86);
        const QImage shot = screen();
        // The marker's outline goes round cell 86 and its neighbours,
        // which show whole inside it. It lies over the edges of the
        // cells one further out: 64 to 68, 74, 78, 84, 88, 94, 98 and
        // 104 to 108. The pulse has gone and left nothing behind.
        // The dashed outline goes round cell 14, the last emptied, and
        // lies over 2, 6, 12, 16, 22, 26 and 32 to 36.
        QCOMPARE(view->sourceCell(), 14);
        QVERIFY(!view->isPulsing());
        for (const auto &[first, last] : {std::pair{0, 1}, {3, 5}, {7, 11}, {13, 15}, {17, 21},
                                          {23, 25}, {27, 31}, {37, 63}})
            QCOMPARE(unsettled(shot, first, last), 0);
        QCOMPARE(unsettled(shot, 0, 63), 11);
        QCOMPARE(unsettled(shot, 69, 73), 0);
        QCOMPARE(unsettled(shot, 75, 77), 0);
        QCOMPARE(unsettled(shot, 79, 83), 0);
        QCOMPARE(unsettled(shot, 85, 87), 0);
        QCOMPARE(unsettled(shot, 89, 93), 0);
        QCOMPARE(unsettled(shot, 95, 97), 0);
        QCOMPARE(unsettled(shot, 99, 103), 0);
        QCOMPARE(unsettled(shot, 109, kColumns * rows - 1), 0);
        QVERIFY(unsettled(shot, 64, 68) > 0);
    }

    // DFR-0018 INV-10: with the desktop's animations off, a move shows at
    // once as settled patterns, nothing flies, and a new map does not
    // sweep in.
    void motionOffChangesAtOnce()
    {
        DriveMap map = rowsMap();
        const auto view = makeView();
        view->setMotion(false);
        view->showMap(&map);
        QVERIFY(!view->isMoving());
        QVERIFY(samePixels(view->grab().toImage(), cellAt(39), cellImage(m_theme, CellState::Free)));

        const BlocksMoved blocks{{{10, 2}}, {{38, 2}}};
        map.move(blocks);
        view->showMove(blocks);
        QCOMPARE(view->motesInFlight(), 0);
        QCOMPARE(view->cells(), map.cells(view->capacity()));
        const QImage shot = view->grab().toImage();
        for (const int cell : {10, 11})
            QVERIFY(samePixels(shot, cellAt(cell), cellImage(m_theme, CellState::Free)));
        // Both show whole inside the marker's outline, which does not
        // pulse.
        for (const int cell : {38, 39})
            QVERIFY(samePixels(shot, cellAt(cell), cellImage(m_theme, CellState::Whole)));
        QCOMPARE(view->markerCell(), 39);
        QVERIFY(!view->isPulsing());
        QVERIFY(!view->isMoving());
    }

    // DFR-0098, S10: the marker's pulse closes in when the marker moves,
    // and begins at most once a second however fast the moves come.
    void markerPulsesAtMostOnceASecond()
    {
        DriveMap map = rowsMap();
        const auto view = settledViewOf(map);
        QVERIFY(!view->isPulsing());
        int begun = 0;
        bool pulsing = false;
        const qint64 start = m_now;
        // A move every 100 ms for three seconds, each to and fro.
        for (int step = 0; m_now < start + 3000; ++step) {
            const BlocksMoved blocks = step % 2 == 0 ? BlocksMoved{{{10, 1}}, {{30, 1}}}
                                                     : BlocksMoved{{{30, 1}}, {{10, 1}}};
            map.move(blocks);
            view->showMove(blocks);
            for (int tick = 0; tick < 10; ++tick) {
                m_now += 10;
                view->followMap();
                const bool now = view->isPulsing();
                begun += now && !pulsing;
                pulsing = now;
            }
        }
        QVERIFY2(begun >= 2 && begun <= 3, qPrintable(QString::number(begun)));
        QVERIFY(view->markerCell() >= 0);
        m_now += DriveMapView::kPulseMs;
        QVERIFY(!view->isPulsing());
    }

    // DFR-0105, S10: a dashed outline goes round the cell last emptied
    // and its neighbours. Its line shows the marker's colour in dashes
    // and the marker's edge between them, so it reads apart from the
    // marker's solid line without colour.
    void sourceOutlineIsDashed()
    {
        DriveMap map = rowsMap();
        const auto view = makeView();
        view->setMotion(false);
        view->showMap(&map);
        QCOMPARE(view->sourceCell(), -1);
        const BlocksMoved blocks{{{10, 2}}, {{38, 2}}};
        map.move(blocks);
        view->showMove(blocks);
        QCOMPARE(view->sourceCell(), 11);
        QCOMPARE(view->markerCell(), 39);

        const QImage shot = view->grab().toImage();
        const QList<CellState> cells = map.cells(view->capacity());
        // The outline's pixels over a cell, of the line's colour and of
        // its edge's.
        const auto line = [&](int cell) {
            const QImage settled = cellImage(m_theme, cells[cell]);
            std::pair<int, int> count{0, 0};
            for (int y = 0; y < settled.height(); ++y)
                for (int x = 0; x < settled.width(); ++x) {
                    const QRgb pixel = shot.pixelColor(cellAt(cell) + QPoint(x, y)).rgb();
                    if (pixel == settled.pixelColor(x, y).rgb())
                        continue;
                    count.first += pixel == m_theme.markerColour().rgb();
                    count.second += pixel == m_theme.markerEdge().rgb();
                }
            return count;
        };
        // Cell 13 lies under the dashed outline's right side, cell 27
        // under the marker's left side: the same length of line each.
        const auto [dashes, gaps] = line(13);
        const auto [solid, edge] = line(27);
        QVERIFY(solid > 0);
        QVERIFY2(dashes > 0 && dashes < solid, qPrintable(QString::number(dashes)));
        QVERIFY2(gaps > edge, qPrintable(QString::number(gaps)));
        // The cells inside it show their own patterns.
        for (const int cell : {0, 1, 2, 10, 11, 12, 20, 21, 22})
            QVERIFY2(samePixels(shot, cellAt(cell), cellImage(m_theme, cells[cell])),
                     qPrintable(QString::number(cell)));

        view->showMap(&map);
        QCOMPARE(view->sourceCell(), -1);
    }

    // No map: no cells, and no frame timer waking to sweep them in.
    void noMapShowsNothing()
    {
        const auto view = settledView();
        view->showMap(nullptr);
        QVERIFY(!view->isMoving());
        QCOMPARE(view->cells(), QList<CellState>{});
        QVERIFY(samePixels(view->grab().toImage(), cellAt(0), blankImage(m_theme)));
    }
};

QTEST_MAIN(TestDriveMapView)
#include "tst_drivemapview.moc"
