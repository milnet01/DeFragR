// DriveMapView: the drive as a grid of cells, first block top left (top
// right in a right-to-left language), painted with the theme's patterns
// (S4, S10). A cell never jumps: each change fades in while the cell
// swells and glows past its edges, then settles, from one frame timer that
// runs only while something moves. A new map fills in as a bright band
// sweeping from the first block to the last. A cell glows once per change
// and starts a change at most every kChangeGapMs, so nothing flashes more
// than three times a second (S10, WCAG 2.2 SC 2.3.1), whatever feeds it.
//
// During a defrag each move plays as a journey: the cells it left glow
// and fade, small blocks fly on a bowed path to the cells it reached, and
// those change as the blocks land. Moves gather into launches, so the map
// never falls far behind the engine, and each frame repaints only what
// moved (docs/specs/DFR-0018-window-folder-defrag.md § 4.3). With the
// desktop's animations off, cells change at once and nothing flies.
#pragma once

#include "model/DriveMap.h"

#include <QElapsedTimer>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QRegion>
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
    // A defrag's moves: the longest wait between launches, how long a
    // small block flies, how long from a launch's first landing to its
    // last, and the most small blocks one launch sends.
    static constexpr int kLaunchGapMs = 150;
    static constexpr int kFlightMs = 600;
    static constexpr int kLandSpreadMs = 200;
    static constexpr int kMotesMax = 16;
    // The marker's pulse: a second outline that closes in on the marker
    // from kPulseReach cells further out, how long it takes, and the
    // least time from one pulse's start to the next.
    static constexpr int kPulseMs = 600;
    static constexpr int kPulseGapMs = 1000;
    static constexpr int kPulseReach = 3;

    explicit DriveMapView(const Theme &theme, QWidget *parent = nullptr);

    // Shows this map, filling in as a sweep. A resize fits it to as
    // many cells as then fit, all settled at once: a resize starts no glow.
    // The map must outlive the view, or the next call; nullptr shows
    // nothing.
    void showMap(const model::DriveMap *map);

    // The cells that fit at the view's size, and the ones shown.
    int capacity() const;
    QList<model::CellState> cells() const;

    // For a demo or a test with cells of its own; a defrag uses showMove.
    // Shows these cells instead, each one that differs fading from what
    // it shows now. A cell whose last change began less than kChangeGapMs
    // ago waits until then, and then shows the last state asked for. A
    // resize shows the map again, without these changes.
    void changeCells(const QList<model::CellState> &cells);

    // A move the shown map has taken: its journey joins the next launch.
    void showMove(const engine::BlocksMoved &move);
    // The shown map changed: the cells follow it at the next launch.
    void followMap();

    // Off: cells change at once, with no fade, glow, sweep or small
    // blocks. Starts as the desktop's animations setting says, and
    // follows it when the style changes.
    void setMotion(bool on);
    bool motion() const { return m_motion; }

    // Whether the frame timer runs.
    bool isMoving() const { return m_frame.isActive(); }
    // The small blocks in flight, and the cell the marker rests on, or -1.
    int motesInFlight() const;
    int markerCell() const { return int(m_marker); }
    // Whether the marker's pulse is closing in.
    bool isPulsing() const;

    // The clock the motion reads, in milliseconds; tests set their own.
    void setClock(std::function<qint64()> now);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Cell {
        std::optional<model::CellState> from;  // none: blank ground
        model::CellState to;
        qint64 start;  // when the change begins
        int length;    // how long it lasts
        // A change waiting for kChangeGapMs to pass, from `to` at thenAt.
        std::optional<model::CellState> then = std::nullopt;
        qint64 thenAt = 0;
        int thenLength = kChangeMs;
        // A cell a move left: it glows in its old pattern before it fades.
        bool leaving = false;
        bool thenLeaving = false;

        // The cell at `now`: its waiting change, once begun.
        Cell at(qint64 now) const;
    };

    // Fits the map to as many cells as the view holds: sweeping from
    // m_sweepStart, or settled.
    void fitMapToCells(bool sweep);
    // One small block's flight: from, round the bend, to.
    struct Mote {
        QPointF from;
        QPointF bend;
        QPointF to;
        qint64 start;

        // Where it is at `now`, and `back` of its flight earlier.
        QPointF at(qint64 now, qreal back = 0) const;
    };

    // Runs the frame timer until m_stillAt; repaints the whole view, or
    // leaves that to the frames.
    void moveUntil(qint64 stillAt, bool repaintAll = true);
    void frame();
    // Brings the cells to the map: every cell that differs from what it
    // is due to show is given a change, and the waiting moves fly.
    void launch(qint64 now);
    // Gives `cell`, taken at `now`, a change to `state` beginning at
    // `when`, or later where kChangeGapMs says. Returns when it begins.
    qint64 schedule(Cell &cell, model::CellState state, qint64 when, bool leaving) const;
    // The cells the runs lie in, added to `cells` up to its limit.
    void noteCells(QList<qsizetype> &cells, const QList<engine::MapRun> &runs) const;
    // What changed on screen between two frames.
    QRegion movedBetween(qint64 since, qint64 now) const;
    // The marker's outline, at rest; and its pulse's at `now`, or none.
    QRect markerRect() const;
    QRect pulseRect(qint64 now) const;
    // The four strips an outline of the marker's width covers, edge
    // included.
    QRegion outline(const QRect &rect) const;
    void paintOutline(QPainter &painter, const QRect &rect) const;
    int changeLength() const { return m_motion ? kChangeMs : 0; }
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
    qint64 m_lastFrame = 0;
    bool m_motion = true;
    // The cells the moves waiting for a launch left and reached.
    QList<qsizetype> m_fromCells;
    QList<qsizetype> m_toCells;
    bool m_launchDue = false;
    qint64 m_launchAt = 0;
    std::optional<qint64> m_lastLaunch;
    QList<Mote> m_motes;
    qsizetype m_marker = -1;
    std::optional<qint64> m_pulseStart;
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
