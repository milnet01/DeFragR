#include "ui/DriveMapView.h"

#include "ui/Theme.h"

#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineF>
#include <QPaintEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QSet>
#include <QStyle>
#include <QStyleHints>
#include <QtMath>
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#include <QAccessibilityHints>
#endif

#include <algorithm>
#include <limits>
#include <type_traits>

using defragr::model::CellState;

namespace defragr::ui {

namespace {

// How many cells a glowing cell's glow reaches past each of its edges.
constexpr int kGlowReach = 1;

// The most cells one launch's small blocks choose between, at each end.
// Moves past it still change their cells; they only send no small block.
constexpr qsizetype kWaitingCellsMax = 256;

// Whether the desktop allows motion. Qt 6.12 reads the desktop's setting
// itself. Before it, the style says how long its own animations last,
// and Breeze gives 0 when KDE's animations are off.
bool desktopAllowsMotion(const QWidget *widget)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    if (QGuiApplication::styleHints()->accessibility()->motionPreference()
        == Qt::MotionPreference::ReducedMotion)
        return false;
#endif
    return widget->style()->styleHint(QStyle::SH_Widget_Animation_Duration, nullptr, widget) != 0;
}

// Device pixels for `side` at `ratio`, rounded up so the cell fits.
QSize devicePixels(int side, qreal ratio)
{
    const int pixels = qCeil(side * ratio);
    return {pixels, pixels};
}

QPixmap paintedCell(const Theme &theme, CellState state, int side, qreal ratio)
{
    QPixmap pixmap(devicePixels(side, ratio));
    pixmap.setDevicePixelRatio(ratio);
    // Rounding up can leave device pixels past the cell; they show the map.
    pixmap.fill(theme.mapBackground());
    QPainter painter(&pixmap);
    theme.paintCell(painter, QRect(0, 0, side, side), state);
    return pixmap;
}

// A cell at the peak of its glow, reaching kGlowReach cells past each
// edge: the theme's soft round halo, and over it the cell half as big
// again, washed with the theme's glow.
QPixmap paintedGlow(const Theme &theme, CellState state, int side, qreal ratio)
{
    const int across = (2 * kGlowReach + 1) * side;
    QPixmap pixmap(devicePixels(across, ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QRadialGradient gradient(QPointF(across, across) / 2, across / 2.0);
    gradient.setStops(theme.glowHalo(state));
    QPainter painter(&pixmap);
    painter.fillRect(QRect(0, 0, across, across), gradient);

    const int swollen = side * 3 / 2;
    const QRect cell((across - swollen) / 2, (across - swollen) / 2, swollen, swollen);
    theme.paintCell(painter, cell, state);
    painter.setOpacity(Theme::kGlowWashOpacity);
    painter.fillRect(cell, theme.glowWash(state));
    return pixmap;
}

}  // namespace

DriveMapView::DriveMapView(const Theme &theme, QWidget *parent)
    : QWidget(parent), m_theme(theme)
{
    m_elapsed.start();
    m_now = [this] { return m_elapsed.elapsed(); };
    // About 60 frames a second.
    m_frame.setInterval(16);
    m_frame.setTimerType(Qt::PreciseTimer);
    connect(&m_frame, &QTimer::timeout, this, &DriveMapView::frame);
    setAccessibleName(tr("Drive map"));
    m_motion = desktopAllowsMotion(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    connect(QGuiApplication::styleHints()->accessibility(),
            &QAccessibilityHints::motionPreferenceChanged, this,
            [this] { setMotion(desktopAllowsMotion(this)); });
#endif
}

void DriveMapView::showMap(const model::DriveMap *map)
{
    m_map = map;
    m_sweepStart = m_now();
    fitMapToCells(m_motion);
}

int DriveMapView::capacity() const
{
    const int rows = std::max(1, (height() + m_theme.cellGap()) / pitch());
    return columns() * rows;
}

QList<CellState> DriveMapView::cells() const
{
    QList<CellState> states;
    states.reserve(m_cells.size());
    const qint64 now = m_now();
    for (const Cell &cell : m_cells)
        states.append(cell.at(now).to);
    return states;
}

namespace {

// A change runs from 0, not begun, to 1, done. The new state fades in
// over its first part; the glow peaks soon after it begins, then dies away
// over the rest, so the cell flashes once and settles.
constexpr qreal kFadePart = 0.35;
constexpr qreal kGlowPeak = 0.2;

// How far a change from `start` lasting `length` has got at `now`. One
// of no length is done the moment it starts.
qreal progress(qint64 start, int length, qint64 now)
{
    if (length <= 0)
        return now >= start ? 1 : 0;
    return std::clamp(qreal(now - start) / length, 0.0, 1.0);
}

// QEasingCurve's OutCubic and OutQuad, written out: a QEasingCurve
// allocates, and the map eases every cell every frame.
qreal outCubic(qreal x)
{
    const qreal left = 1 - x;
    return 1 - left * left * left;
}

qreal outQuad(qreal x) { return x * (2 - x); }

// How much of the new state shows, eased: 0 none, 1 all of it.
qreal fadeIn(qreal t) { return outCubic(std::min(1.0, t / kFadePart)); }

// The same for a cell a move left: it glows in its old pattern first,
// and has faded by kChangeGapMs, when its next change may begin.
constexpr qreal kLeaveFadeFrom = 0.15;
constexpr qreal kLeaveFadePart = 0.22;
static_assert((kLeaveFadeFrom + kLeaveFadePart) * DriveMapView::kChangeMs
              <= DriveMapView::kChangeGapMs);
qreal leaveFadeIn(qreal t)
{
    return outCubic(std::clamp((t - kLeaveFadeFrom) / kLeaveFadePart, 0.0, 1.0));
}

// Slow, fast, slow: a small block's place along its flight.
qreal easeInOut(qreal x) { return x * x * (3 - 2 * x); }

// How far behind each other a small block's trail lies, as a part of
// its flight, and how many squares the trail has.
constexpr qreal kTrailStep = 0.02;
constexpr int kTrailSquares = 6;

// How brightly the cell glows: 0 not at all, 1 at its peak.
qreal glowAt(qreal t)
{
    if (t <= 0 || t >= 1)
        return 0;
    if (t < kGlowPeak)
        return outQuad(t / kGlowPeak);
    const qreal left = 1 - (t - kGlowPeak) / (1 - kGlowPeak);
    return left * left;
}

}  // namespace

DriveMapView::Cell DriveMapView::Cell::at(qint64 now) const
{
    if (!then || now < thenAt)
        return *this;
    Cell next{to, *then, thenAt, thenLength};
    next.leaving = thenLeaving;
    return next;
}

QPointF DriveMapView::Mote::at(qint64 now, qreal back) const
{
    const qreal u = easeInOut(std::clamp(progress(start, kFlightMs, now) - back, 0.0, 1.0));
    const qreal left = 1 - u;
    return left * left * from + 2 * left * u * bend + u * u * to;
}

int DriveMapView::motesInFlight() const
{
    const qint64 now = m_now();
    return int(std::ranges::count_if(
        m_motes, [now](const Mote &mote) { return now < mote.start + kFlightMs; }));
}

void DriveMapView::setMotion(bool on)
{
    if (m_motion == on)
        return;
    m_motion = on;
    if (!on) {
        // Whatever was moving is where it was going.
        m_motes.clear();
        const qint64 now = m_now();
        for (Cell &cell : m_cells) {
            cell = cell.at(now);
            cell.start = std::min(cell.start, now);
            cell.length = 0;
            cell.thenLength = 0;
        }
    }
    update();
}

void DriveMapView::noteCells(QList<qsizetype> &cells, const QList<engine::MapRun> &runs) const
{
    const quint64 perCell = m_map->blocksPerCell(m_fittedFor);
    if (perCell == 0)
        return;
    for (const engine::MapRun &run : runs) {
        if (run.length == 0)
            continue;
        const quint64 first = run.start / perCell;
        // The decoder takes any start and length, so the end saturates.
        const quint64 lastBlock = run.length - 1 > std::numeric_limits<quint64>::max() - run.start
                                      ? std::numeric_limits<quint64>::max()
                                      : run.start + run.length - 1;
        const quint64 last = lastBlock / perCell;
        for (quint64 cell = first; cell <= last; ++cell) {
            if (cells.size() >= kWaitingCellsMax || cell >= quint64(m_cells.size()))
                return;
            if (cells.isEmpty() || cells.last() != qsizetype(cell))
                cells.append(qsizetype(cell));
        }
    }
}

void DriveMapView::showMove(const engine::BlocksMoved &move)
{
    if (!m_map)
        return;
    noteCells(m_fromCells, move.from);
    noteCells(m_toCells, move.to);
    followMap();
}

void DriveMapView::followMap()
{
    if (!m_map)
        return;
    const qint64 now = m_now();
    if (!m_launchDue) {
        m_launchDue = true;
        m_launchAt = m_lastLaunch ? std::max(now, *m_lastLaunch + kLaunchGapMs) : now;
    }
    if (!m_motion || now >= m_launchAt)
        launch(now);
    else
        moveUntil(m_launchAt, false);
}

qint64 DriveMapView::schedule(Cell &cell, CellState state, qint64 when, bool leaving) const
{
    cell.then = state;
    cell.thenAt = std::max(when, cell.start + kChangeGapMs);
    cell.thenLength = changeLength();
    cell.thenLeaving = leaving && m_motion;
    return cell.thenAt;
}

void DriveMapView::launch(qint64 now)
{
    m_launchDue = false;
    m_lastLaunch = now;
    const QList<CellState> target = m_map->cells(m_fittedFor);
    if (target.size() != m_cells.size()) {
        // The map grew past its end, so every cell holds other blocks now.
        fitMapToCells(false);
        return;
    }

    // Where each cell stands among the cells left and reached, so the
    // changes ripple in the moves' own order.
    QHash<qsizetype, qsizetype> leftAt;
    QHash<qsizetype, qsizetype> reachedAt;
    for (qsizetype i = 0; i < m_fromCells.size(); ++i)
        leftAt.insert(m_fromCells[i], i);
    for (qsizetype i = 0; i < m_toCells.size(); ++i)
        reachedAt.insert(m_toCells[i], i);
    const bool flying = m_motion && !m_fromCells.isEmpty() && !m_toCells.isEmpty();

    qint64 stillAt = 0;
    for (qsizetype i = 0; i < m_cells.size(); ++i) {
        Cell cell = m_cells[i].at(now);
        // Compared with what the cell is due to show, not what it shows:
        // a change still waiting is part of the picture already.
        if (target[i] == cell.then.value_or(cell.to))
            continue;
        if (target[i] == cell.to) {
            // Back to what it shows: nothing waits, and nothing glows.
            cell.then.reset();
            m_cells[i] = cell;
            continue;
        }
        qint64 when = now;
        bool leaving = false;
        if (!m_motion) {
        } else if (const auto reached = reachedAt.constFind(i); reached != reachedAt.cend()) {
            when = now + (flying ? kFlightMs : 0)
                   + *reached * kLandSpreadMs / m_toCells.size();
        } else if (const auto left = leftAt.constFind(i); left != leftAt.cend()) {
            when = now + *left * kLandSpreadMs / m_fromCells.size();
            leaving = true;
        } else {
            when = now + kFlightMs;
        }
        stillAt = std::max(stillAt, schedule(cell, target[i], when, leaving) + changeLength());
        m_cells[i] = cell;
    }

    if (flying) {
        const qsizetype count = std::clamp<qsizetype>(
            std::max(m_fromCells.size(), m_toCells.size()), 3, kMotesMax);
        for (qsizetype k = 0; k < count; ++k) {
            const QPointF from =
                QRectF(cellRect(m_fromCells[k * m_fromCells.size() / count])).center();
            const QPointF to = QRectF(cellRect(m_toCells[k * m_toCells.size() / count])).center();
            // Bowed to one side of the straight line, each a little
            // differently, so the small blocks fan out and close again.
            const QLineF line(from, to);
            const QLineF across = line.normalVector();
            const qreal bow = std::min(line.length() / 4, 4.0 * pitch()) * (0.6 + 0.2 * (k % 3));
            const QPointF bend = line.center()
                                 + QPointF(across.dx(), across.dy())
                                       * (line.length() > 0 ? bow / line.length() : 0);
            m_motes.append({from, bend, to, now + k * kLandSpreadMs / count});
        }
        stillAt = std::max(stillAt, now + kLandSpreadMs + kFlightMs);
    }
    if (!m_fromCells.isEmpty()) {
        update(outline(outlineRect(m_source)));
        m_source = m_fromCells.last();
        update(outline(outlineRect(m_source)));
    }
    if (!m_toCells.isEmpty()) {
        update(outline(markerRect()));
        m_marker = m_toCells.last();
        update(outline(markerRect()));
        // The pulse shows where the marker has gone. One a second at
        // most: launches come faster than anyone should see flashes.
        if (m_motion && (!m_pulseStart || now >= *m_pulseStart + kPulseGapMs)) {
            m_pulseStart = now;
            stillAt = std::max(stillAt, now + kPulseMs);
        }
    }
    m_fromCells.clear();
    m_toCells.clear();
    if (stillAt > 0)
        moveUntil(stillAt, false);
}

QRect DriveMapView::outlineRect(qsizetype cell) const
{
    if (cell < 0 || cell >= m_cells.size())
        return {};
    const int out = Theme::markerReach * pitch() + m_theme.cellGap() + m_theme.markerWidth() + 1;
    return cellRect(cell).adjusted(-out, -out, out, out);
}

bool DriveMapView::isPulsing() const
{
    return !pulseRect(m_now()).isNull();
}

QRect DriveMapView::pulseRect(qint64 now) const
{
    const QRect rest = markerRect();
    if (rest.isNull() || !m_pulseStart || now < *m_pulseStart || now >= *m_pulseStart + kPulseMs)
        return {};
    const int out = int((1 - progress(*m_pulseStart, kPulseMs, now)) * kPulseReach * pitch());
    return rest.adjusted(-out, -out, out, out);
}

QRegion DriveMapView::outline(const QRect &rect) const
{
    if (rect.isNull())
        return {};
    const int width = m_theme.markerWidth() + 2;
    QRegion strips;
    strips += QRect(rect.left(), rect.top(), rect.width(), width);
    strips += QRect(rect.left(), rect.bottom() - width + 1, rect.width(), width);
    strips += QRect(rect.left(), rect.top(), width, rect.height());
    strips += QRect(rect.right() - width + 1, rect.top(), width, rect.height());
    return strips;
}

void DriveMapView::paintOutline(QPainter &painter, const QRect &rect, bool dashed) const
{
    const int width = m_theme.markerWidth();
    const qreal in = width / 2.0 + 1;
    const QRectF line = QRectF(rect).adjusted(in, in, -in, -in);
    // Rounded outside and square inside, so no corner reaches over the
    // cells it goes round.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(m_theme.markerEdge(), width + 2));
    painter.drawRoundedRect(line, in, in);
    QPen pen(m_theme.markerColour(), width, Qt::SolidLine, Qt::FlatCap);
    if (dashed)
        pen.setDashPattern({2, 2});  // in pen widths
    painter.setPen(pen);
    painter.drawRoundedRect(line, in, in);
}

void DriveMapView::changeCells(const QList<CellState> &cells)
{
    const qint64 now = m_now();
    QList<Cell> next;
    next.reserve(cells.size());
    qint64 stillAt = 0;
    for (qsizetype i = 0; i < cells.size(); ++i) {
        if (i >= m_cells.size()) {
            next.append({std::nullopt, cells[i], now, changeLength()});
            stillAt = now + changeLength();
            continue;
        }
        Cell cell = m_cells[i].at(now);
        if (cell.to == cells[i]) {
            cell.then.reset();  // back to what it shows: nothing waits
        } else if (cell.then || now - cell.start < kChangeGapMs) {
            // Too soon after the last change began: this one waits, and
            // replaces any already waiting.
            if (!cell.then)
                cell.thenAt = cell.start + kChangeGapMs;
            cell.then = cells[i];
            cell.thenLength = changeLength();
            cell.thenLeaving = false;
            stillAt = std::max(stillAt, cell.thenAt + changeLength());
        } else {
            // A change still going starts again from whichever of its
            // two states shows most.
            const std::optional<CellState> shown =
                fadeIn(progress(cell.start, cell.length, now)) >= 0.5 ? cell.to : cell.from;
            cell = {shown, cells[i], now, changeLength()};
            stillAt = now + changeLength();
        }
        next.append(cell);
    }
    m_cells = std::move(next);
    if (stillAt > 0)
        moveUntil(stillAt);
    else
        update();
}

void DriveMapView::setClock(std::function<qint64()> now)
{
    m_now = std::move(now);
}

QSize DriveMapView::sizeHint() const
{
    return {40 * pitch(), 10 * pitch()};
}

QSize DriveMapView::minimumSizeHint() const
{
    return {8 * pitch(), 3 * pitch()};
}

void DriveMapView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    const qint64 now = m_now();
    const int across = columns();
    const qsizetype rowCount = (m_cells.size() + across - 1) / across;
    // The rows to paint: each one the region touches, and, since a
    // glowing cell reaches kGlowReach cells past its edges, as many rows
    // more either side.
    QList<bool> wanted(rowCount, false);
    for (const QRect &rect : event->region()) {
        painter.fillRect(rect, m_theme.mapBackground());
        const qsizetype first = std::max(0, rect.top() / pitch() - kGlowReach);
        const qsizetype last =
            std::min<qsizetype>(rowCount - 1, rect.bottom() / pitch() + kGlowReach);
        for (qsizetype row = first; row <= last; ++row)
            wanted[row] = true;
    }
    // First every cell at its own size, then the glowing ones over their
    // neighbours.
    QList<qsizetype> glowing;
    for (qsizetype row = 0; row < rowCount; ++row) {
        if (!wanted[row])
            continue;
        const qsizetype end = std::min(m_cells.size(), (row + 1) * across);
        for (qsizetype i = row * across; i < end; ++i) {
            const Cell cell = m_cells[i].at(now);
            const QRect rect = cellRect(i);
            const qreal t = progress(cell.start, cell.length, now);
            const qreal shown = cell.leaving ? leaveFadeIn(t) : fadeIn(t);
            if (shown < 1 && cell.from)
                painter.drawPixmap(rect.topLeft(), tile(*cell.from));
            if (shown > 0) {
                painter.setOpacity(shown);
                painter.drawPixmap(rect.topLeft(), tile(cell.to));
                painter.setOpacity(1);
            }
            if (glowAt(t) > 0)
                glowing.append(i);
        }
    }
    const int reach = kGlowReach * m_theme.cellSide();
    for (const qsizetype i : glowing) {
        const Cell cell = m_cells[i].at(now);
        painter.setOpacity(glowAt(progress(cell.start, cell.length, now)));
        // A cell a move left lights up as what it was.
        const CellState lit = cell.leaving && cell.from ? *cell.from : cell.to;
        painter.drawPixmap(cellRect(i).topLeft() - QPoint(reach, reach), glow(lit));
    }
    painter.setOpacity(1);

    // The marker last, so it shows whole where the two outlines cross.
    painter.setRenderHint(QPainter::Antialiasing);
    if (const QRect source = outlineRect(m_source); !source.isNull())
        paintOutline(painter, source, true);
    if (const QRect marker = markerRect(); !marker.isNull()) {
        paintOutline(painter, marker);
        if (const QRect pulse = pulseRect(now); !pulse.isNull()) {
            // Faint far out, full as it closes on the marker.
            painter.setOpacity(0.3 + 0.7 * progress(*m_pulseStart, kPulseMs, now));
            paintOutline(painter, pulse);
            painter.setOpacity(1);
        }
    }
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Each small block leads a trail of fainter, smaller ones.
    const qreal side = m_theme.moteSide();
    for (const Mote &mote : m_motes) {
        const qreal t = progress(mote.start, kFlightMs, now);
        if (now < mote.start || t >= 1)
            continue;
        for (int square = kTrailSquares - 1; square >= 0; --square) {
            const qreal size = side * (1 - 0.1 * square);
            painter.setOpacity(1 - qreal(square) / kTrailSquares);
            const QPointF centre = mote.at(now, square * kTrailStep);
            painter.fillRect(QRectF(centre.x() - size / 2, centre.y() - size / 2, size, size),
                             m_theme.moteColour());
        }
    }
    painter.setOpacity(1);
}

void DriveMapView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_map && capacity() != m_fittedFor)
        fitMapToCells(false);
}

void DriveMapView::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange)
        setMotion(desktopAllowsMotion(this));
}

void DriveMapView::fitMapToCells(bool sweep)
{
    // Cells are counted afresh, so what was aimed at the old ones goes.
    m_motes.clear();
    m_fromCells.clear();
    m_toCells.clear();
    m_launchDue = false;
    m_marker = -1;
    m_source = -1;
    m_pulseStart.reset();
    m_cells.clear();
    m_fittedFor = capacity();
    const qint64 now = m_now();
    if (m_map) {
        const QList<CellState> states = m_map->cells(m_fittedFor);
        m_cells.reserve(states.size());
        for (qsizetype i = 0; i < states.size(); ++i)
            m_cells.append({std::nullopt, states[i],
                            sweep ? m_sweepStart + i * kSweepMs / states.size()
                                  : now - kArriveMs,
                            kArriveMs});
    }
    if (sweep && !m_cells.isEmpty()) {
        moveUntil(m_sweepStart + kSweepMs + kArriveMs);
        return;
    }
    // Every cell has arrived, or there are none, so nothing moves.
    m_stillAt = now;
    m_frame.stop();
    update();
}

void DriveMapView::moveUntil(qint64 stillAt, bool repaintAll)
{
    m_stillAt = std::max(m_stillAt, stillAt);
    const qint64 now = m_now();
    if (now < m_stillAt && !m_frame.isActive()) {
        m_lastFrame = now;
        m_frame.start();
    }
    if (repaintAll)
        update();
    else
        update(movedBetween(now, now));
}

void DriveMapView::frame()
{
    const qint64 now = m_now();
    if (m_launchDue && now >= m_launchAt)
        launch(now);
    update(movedBetween(m_lastFrame, now));
    // A small block that had landed by the last frame is painted over now.
    m_motes.removeIf([this](const Mote &mote) { return mote.start + kFlightMs <= m_lastFrame; });
    m_lastFrame = now;
    if (now >= m_stillAt && !m_launchDue)
        m_frame.stop();
}

QRegion DriveMapView::movedBetween(qint64 since, qint64 now) const
{
    QRegion moved;
    const int across = columns();
    const qsizetype rowCount = (m_cells.size() + across - 1) / across;
    // Whole rows: a cell whose change was under way at any time from
    // `since` to `now`, with the rows its glow reaches.
    qsizetype paintedTo = -1;
    for (qsizetype row = 0; row < rowCount; ++row) {
        const qsizetype end = std::min(m_cells.size(), (row + 1) * across);
        bool changing = false;
        for (qsizetype i = row * across; i < end && !changing; ++i) {
            const Cell cell = m_cells[i].at(now);
            changing = cell.start <= now && cell.start + cell.length >= since;
        }
        if (!changing)
            continue;
        const qsizetype first = std::max(paintedTo + 1, row - kGlowReach);
        const qsizetype last = std::min(rowCount - 1, row + kGlowReach);
        if (first <= last)
            moved += QRect(0, int(first) * pitch(), width(), int(last - first + 1) * pitch());
        paintedTo = std::max(paintedTo, last);
    }
    // The marker's pulse, where it was and where it is, and the marker
    // it uncovers.
    if (m_pulseStart && *m_pulseStart <= now && *m_pulseStart + kPulseMs >= since) {
        moved += outline(pulseRect(std::max(since, *m_pulseStart)));
        moved += outline(pulseRect(now));
        moved += outline(markerRect());
    }
    // Each small block's trail, where it was and where it is.
    const qreal margin = m_theme.moteSide() / 2.0 + 1;
    for (const Mote &mote : m_motes) {
        if (now < mote.start || since >= mote.start + kFlightMs + 1)
            continue;
        QRectF around;
        for (const qint64 when : {since, now})
            for (int square = 0; square < kTrailSquares; ++square) {
                const QPointF at = mote.at(when, square * kTrailStep);
                around |= QRectF(at.x() - margin, at.y() - margin, 2 * margin, 2 * margin);
            }
        moved += around.toAlignedRect();
    }
    return moved;
}

int DriveMapView::pitch() const
{
    return m_theme.cellSide() + m_theme.cellGap();
}

int DriveMapView::columns() const
{
    return std::max(1, (width() + m_theme.cellGap()) / pitch());
}

QRect DriveMapView::cellRect(qsizetype index) const
{
    const int across = columns();
    const int side = m_theme.cellSide();
    int x = int(index % across) * pitch();
    if (layoutDirection() == Qt::RightToLeft)
        x = width() - x - side;
    return {x, int(index / across) * pitch(), side, side};
}

void DriveMapView::keepPaintedFor(int side, qreal ratio)
{
    // Compared as painted, not by the pixmaps' sizes: rounding to whole
    // device pixels changes those, and every paint would paint them anew.
    if (side == m_paintedSide && ratio == m_paintedRatio)
        return;
    m_tiles = {};
    m_glows = {};
    m_paintedSide = side;
    m_paintedRatio = ratio;
}

const QPixmap &DriveMapView::tile(CellState state)
{
    keepPaintedFor(m_theme.cellSide(), devicePixelRatioF());
    QPixmap &pixmap = m_tiles[size_t(state)];
    if (pixmap.isNull())
        pixmap = paintedCell(m_theme, state, m_paintedSide, m_paintedRatio);
    return pixmap;
}

const QPixmap &DriveMapView::glow(CellState state)
{
    keepPaintedFor(m_theme.cellSide(), devicePixelRatioF());
    QPixmap &pixmap = m_glows[size_t(state)];
    if (pixmap.isNull())
        pixmap = paintedGlow(m_theme, state, m_paintedSide, m_paintedRatio);
    return pixmap;
}

namespace {

// One pattern in the map's key, painted at every paint, so it stays sharp
// on a screen of any scale, and when the window moves to another.
class Swatch : public QWidget {
public:
    Swatch(const Theme &theme, CellState state) : m_theme(theme), m_state(state)
    {
        setFixedSize(theme.cellSide(), theme.cellSide());
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        m_theme.paintCell(painter, rect(), m_state);
    }

private:
    const Theme &m_theme;
    CellState m_state;
};

}  // namespace

QWidget *mapKey(const Theme &theme, QWidget *parent)
{
    auto *key = new QWidget(parent);
    auto *layout = new QHBoxLayout(key);
    layout->setContentsMargins({});
    const std::pair<CellState, QString> entries[] = {
        {CellState::Whole, DriveMapView::tr("Whole files")},
        {CellState::LargePieces, DriveMapView::tr("Files in large pieces")},
        {CellState::Fragmented, DriveMapView::tr("Fragmented files")},
        {CellState::Filesystem, DriveMapView::tr("Folders and filesystem")},
        {CellState::Free, DriveMapView::tr("Free space")},
    };
    static_assert(std::extent_v<decltype(entries)> == model::kCellStates,
                  "the key names every state");
    for (const auto &[state, name] : entries) {
        auto *swatch = new Swatch(theme, state);
        auto *label = new QLabel(name);
        label->setBuddy(swatch);
        layout->addWidget(swatch);
        layout->addWidget(label);
        layout->addSpacing(theme.cellSide());
    }
    layout->addStretch();
    return key;
}

}  // namespace defragr::ui
