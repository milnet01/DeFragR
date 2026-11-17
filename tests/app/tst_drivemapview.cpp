// DriveMapView: the map painted with the theme's patterns (S4, S10). A
// cell never jumps: it fades from old to new, a new map fills in first
// block to last, and the frame timer stops once nothing moves. The view
// reads a clock this test sets, so every frame is a chosen moment.
#include "ui/DriveMapView.h"
#include "ui/Theme.h"

#include <QImage>
#include <QPainter>
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
        view->resize(kColumns * pitch() - m_theme.cellGap(), kRows * pitch() - m_theme.cellGap());
        view->show();
        return view;
    }

    // A view showing the map, its sweep over.
    std::unique_ptr<DriveMapView> settledView()
    {
        auto view = makeView();
        view->showMap(&m_map);
        m_now += DriveMapView::kSweepMs + DriveMapView::kEaseMs;
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

    // Halfway through the sweep, the first cell is in and the last is
    // still blank.
    void sweepGoesFirstToLast()
    {
        const auto view = makeView();
        view->showMap(&m_map);
        QVERIFY(samePixels(view->grab().toImage(), cellAt(0), blankImage(m_theme)));
        m_now = DriveMapView::kSweepMs / 2;
        const QImage shot = view->grab().toImage();
        QVERIFY(samePixels(shot, cellAt(0), cellImage(m_theme, CellState::Whole)));
        QVERIFY(samePixels(shot, cellAt(kColumns * kRows - 1), blankImage(m_theme)));
    }

    // A changed cell shows its old pattern, then a blend of both, then
    // its new one; its neighbours never change.
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
        m_now += DriveMapView::kEaseMs / 2;
        shot = view->grab().toImage();
        QVERIFY(!samePixels(shot, cellAt(0), before));
        QVERIFY(!samePixels(shot, cellAt(0), after));
        QVERIFY(samePixels(shot, cellAt(1), before));
        m_now += DriveMapView::kEaseMs / 2;
        shot = view->grab().toImage();
        QVERIFY(samePixels(shot, cellAt(0), after));
        QCOMPARE(view->cells(), cells);
    }

    // The frame timer runs only while something moves.
    void frameTimerStopsWhenStill()
    {
        const auto view = makeView();
        view->showMap(&m_map);
        QVERIFY(view->isMoving());
        m_now += DriveMapView::kSweepMs + DriveMapView::kEaseMs;
        QTRY_VERIFY(!view->isMoving());
        // Asking for what is already shown moves nothing.
        view->changeCells(view->cells());
        QVERIFY(!view->isMoving());
    }

    // In a right-to-left language the first block is top right.
    void rightToLeftStartsTopRight()
    {
        const auto view = makeView();
        view->setLayoutDirection(Qt::RightToLeft);
        view->showMap(&m_map);
        m_now += DriveMapView::kSweepMs + DriveMapView::kEaseMs;
        const QImage shot = view->grab().toImage();
        const QPoint topRight(view->width() - m_theme.cellSide(), 0);
        QVERIFY(samePixels(shot, topRight, cellImage(m_theme, CellState::Whole)));
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

    void noMapShowsNothing()
    {
        const auto view = settledView();
        view->showMap(nullptr);
        QCOMPARE(view->cells(), QList<CellState>{});
        QVERIFY(samePixels(view->grab().toImage(), cellAt(0), blankImage(m_theme)));
    }
};

QTEST_MAIN(TestDriveMapView)
#include "tst_drivemapview.moc"
