// DriveMapView: the drive as a grid of cells, first block top left (top
// right in a right-to-left language), painted with the theme's patterns
// (S4, S10). A cell never jumps: each change fades in while the cell
// swells and glows past its edges, then settles, from one frame timer that
// runs only while something moves. A new map fills in as a bright band
// sweeping from the first block to the last. A cell glows once per change
// and starts a change at most every kChangeGapMs, so nothing flashes more
// than three times a second (S10, WCAG 2.2 SC 2.3.1), whatever feeds it.
#pragma once

#include "model/DriveMap.h"

#include <QElapsedTimer>
#include <QList>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include <array>
#include <functional>
#include <optional>

namespace defragr::ui {

class Theme;

class DriveMapView : public QWidget {
    Q_OBJECT

public:
    // In milliseconds: how long a changed cell takes to settle, how long
    // a cell arriving in a sweep takes, and how long a sweep takes to
    // reach the last cell.
    static constexpr int kChangeMs = 900;
    static constexpr int kChangeGapMs = 334;  // so at most three a second
    static constexpr int kArriveMs = 500;
    static constexpr int kSweepMs = 2000;

    explicit DriveMapView(const Theme &theme, QWidget *parent = nullptr);

    // Shows this map, filling in as a sweep. A resize fits it to as
    // many cells as then fit, all settled at once: a resize starts no glow.
    // The map must outlive the view, or the next call; nullptr shows
    // nothing.
    void showMap(const model::DriveMap *map);

    // The cells that fit at the view's size, and the ones shown.
    int capacity() const;
    QList<model::CellState> cells() const;

    // Shows these cells instead, each one that differs fading from what
    // it shows now. A cell whose last change began less than kChangeGapMs
    // ago waits until then, and then shows the last state asked for. A
    // resize shows the map again, without these changes.
    void changeCells(const QList<model::CellState> &cells);

    // Whether the frame timer runs.
    bool isMoving() const { return m_frame.isActive(); }

    // The clock the motion reads, in milliseconds; tests set their own.
    void setClock(std::function<qint64()> now);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Cell {
        std::optional<model::CellState> from;  // none: blank ground
        model::CellState to;
        qint64 start;  // when the change begins
        int length;    // how long it lasts
        // A change waiting for kChangeGapMs to pass, from `to` at thenAt.
        std::optional<model::CellState> then = std::nullopt;
        qint64 thenAt = 0;

        // The cell at `now`: its waiting change, once begun.
        Cell at(qint64 now) const;
    };

    // Fits the map to as many cells as the view holds: sweeping from
    // m_sweepStart, or settled.
    void fitMapToCells(bool sweep);
    // Runs the frame timer until m_stillAt.
    void moveUntil(qint64 stillAt);
    void frame();
    int pitch() const;
    int columns() const;
    QRect cellRect(qsizetype index) const;
    // Empties the painted cells unless they were painted at this size
    // and ratio.
    void keepPaintedFor(int side, qreal ratio);
    const QPixmap &tile(model::CellState state);
    const QPixmap &glow(model::CellState state);

    const Theme &m_theme;
    const model::DriveMap *m_map = nullptr;
    QList<Cell> m_cells;
    int m_fittedFor = -1;  // the capacity m_cells was fitted to
    qint64 m_sweepStart = 0;
    qint64 m_stillAt = 0;
    QElapsedTimer m_elapsed;
    std::function<qint64()> m_now;
    QTimer m_frame;
    // Each state's cell, painted once per size and pixel ratio: the ones
    // held, m_paintedSide and m_paintedRatio.
    int m_paintedSide = -1;
    qreal m_paintedRatio = 0;
    std::array<QPixmap, model::kCellStates> m_tiles;
    // Each state's cell at the peak of its glow, painted once per size and
    // pixel ratio.
    std::array<QPixmap, model::kCellStates> m_glows;
};

// The key to the map: each pattern beside its name, at the map's own
// cell size, to sit next to the map where a magnifier finds both.
QWidget *mapKey(const Theme &theme, QWidget *parent = nullptr);

}  // namespace defragr::ui
