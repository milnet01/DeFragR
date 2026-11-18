#include "ui/DriveMapView.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <type_traits>

using defragr::model::CellState;

namespace defragr::ui {

namespace {

// How many cells a glowing cell's glow reaches past each of its edges.
constexpr int kGlowReach = 1;

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
}

void DriveMapView::showMap(const model::DriveMap *map)
{
    m_map = map;
    m_sweepStart = m_now();
    fitMapToCells(true);
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

// How far a change from `start` lasting `length` has got at `now`.
qreal progress(qint64 start, int length, qint64 now)
{
    return std::clamp(qreal(now - start) / length, 0.0, 1.0);
}

// How much of the new state shows, eased: 0 none, 1 all of it.
qreal fadeIn(qreal t)
{
    return QEasingCurve(QEasingCurve::OutCubic).valueForProgress(std::min(1.0, t / kFadePart));
}

// How brightly the cell glows: 0 not at all, 1 at its peak.
qreal glowAt(qreal t)
{
    if (t <= 0 || t >= 1)
        return 0;
    if (t < kGlowPeak)
        return QEasingCurve(QEasingCurve::OutQuad).valueForProgress(t / kGlowPeak);
    const qreal left = 1 - (t - kGlowPeak) / (1 - kGlowPeak);
    return left * left;
}

}  // namespace

DriveMapView::Cell DriveMapView::Cell::at(qint64 now) const
{
    if (!then || now < thenAt)
        return *this;
    return {to, *then, thenAt, kChangeMs};
}

void DriveMapView::changeCells(const QList<CellState> &cells)
{
    const qint64 now = m_now();
    QList<Cell> next;
    next.reserve(cells.size());
    qint64 stillAt = 0;
    for (qsizetype i = 0; i < cells.size(); ++i) {
        if (i >= m_cells.size()) {
            next.append({std::nullopt, cells[i], now, kChangeMs});
            stillAt = now + kChangeMs;
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
            stillAt = std::max(stillAt, cell.thenAt + kChangeMs);
        } else {
            // A change still going starts again from whichever of its
            // two states shows most.
            const std::optional<CellState> shown =
                fadeIn(progress(cell.start, cell.length, now)) >= 0.5 ? cell.to : cell.from;
            cell = {shown, cells[i], now, kChangeMs};
            stillAt = now + kChangeMs;
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
    painter.fillRect(event->rect(), m_theme.mapBackground());
    const qint64 now = m_now();
    const int across = columns();
    // A glowing cell reaches kGlowReach cells past its edges, so take as
    // many rows more either side of what needs painting.
    const qsizetype first = std::max(
        qsizetype(0), qsizetype(event->rect().top() / pitch() - kGlowReach) * across);
    const qsizetype end = std::min(
        m_cells.size(), qsizetype(event->rect().bottom() / pitch() + 1 + kGlowReach) * across);
    // First every cell at its own size, then the glowing ones over their
    // neighbours.
    QList<qsizetype> glowing;
    for (qsizetype i = first; i < end; ++i) {
        const Cell cell = m_cells[i].at(now);
        const QRect rect = cellRect(i);
        const qreal t = progress(cell.start, cell.length, now);
        const qreal shown = fadeIn(t);
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
    const int reach = kGlowReach * m_theme.cellSide();
    for (const qsizetype i : glowing) {
        const Cell cell = m_cells[i].at(now);
        painter.setOpacity(glowAt(progress(cell.start, cell.length, now)));
        painter.drawPixmap(cellRect(i).topLeft() - QPoint(reach, reach), glow(cell.to));
    }
    painter.setOpacity(1);
}

void DriveMapView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_map && capacity() != m_fittedFor)
        fitMapToCells(false);
}

void DriveMapView::fitMapToCells(bool sweep)
{
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
    if (sweep) {
        moveUntil(m_sweepStart + kSweepMs + kArriveMs);
        return;
    }
    // Every cell has arrived, so nothing moves.
    m_stillAt = now;
    update();
}

void DriveMapView::moveUntil(qint64 stillAt)
{
    m_stillAt = std::max(m_stillAt, stillAt);
    if (m_now() < m_stillAt)
        m_frame.start();
    update();
}

void DriveMapView::frame()
{
    update();
    if (m_now() >= m_stillAt)
        m_frame.stop();
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
