#include "ui/DriveMapView.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QRadialGradient>

#include <algorithm>

using defragr::model::CellState;

namespace defragr::ui {

namespace {

QPixmap paintedCell(const Theme &theme, CellState state, int side, qreal ratio)
{
    QPixmap pixmap(QSize(side, side) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    QPainter painter(&pixmap);
    theme.paintCell(painter, QRect(0, 0, side, side), state);
    return pixmap;
}

// A cell at the peak of its glow, three cells across: a soft round halo in
// the state's colour, and over it the cell half as big again and lit up.
// On a dark map the halo is lightened and the cell lit with white, so both
// read as light rather than as a stain; on a light map, with the colour.
QPixmap paintedGlow(const Theme &theme, CellState state, int side, qreal ratio)
{
    const int across = 3 * side;
    QPixmap pixmap(QSize(across, across) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    const bool dark = theme.mapBackground().lightness() < 128;
    QColor colour = theme.cellColour(state);
    if (dark)
        colour = colour.lighter(150);
    QRadialGradient gradient(QPointF(across, across) / 2, across / 2.0);
    gradient.setColorAt(0, colour);
    colour.setAlpha(110);
    gradient.setColorAt(0.45, colour);
    colour.setAlpha(0);
    gradient.setColorAt(1, colour);
    QPainter painter(&pixmap);
    painter.fillRect(QRect(0, 0, across, across), gradient);

    const int swollen = side * 3 / 2;
    const QRect cell((across - swollen) / 2, (across - swollen) / 2, swollen, swollen);
    theme.paintCell(painter, cell, state);
    painter.setOpacity(0.45);
    painter.fillRect(cell, dark ? QColor(Qt::white) : theme.cellColour(state));
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
    sortMap();
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
    for (const Cell &cell : m_cells)
        states.append(cell.to);
    return states;
}

namespace {

// A change runs from 0, not begun, to 1, done. The new state fades in
// over its first part; the glow peaks soon after it begins, then dies away
// over the rest, so the cell flashes once and settles.
constexpr qreal kFadePart = 0.35;
constexpr qreal kGlowPeak = 0.2;

qreal elapsed(qint64 start, int length, qint64 now)
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

void DriveMapView::changeCells(const QList<CellState> &cells)
{
    const qint64 now = m_now();
    QList<Cell> next;
    next.reserve(cells.size());
    bool eased = false;
    for (qsizetype i = 0; i < cells.size(); ++i) {
        if (i < m_cells.size() && m_cells[i].to == cells[i]) {
            next.append(m_cells[i]);
            continue;
        }
        // A change still going starts again from whichever of its two
        // states shows most.
        std::optional<CellState> shown;
        if (i < m_cells.size()) {
            const Cell &old = m_cells[i];
            shown = fadeIn(elapsed(old.start, old.length, now)) >= 0.5 ? old.to : old.from;
        }
        next.append({shown, cells[i], now, kChangeMs});
        eased = true;
    }
    m_cells = std::move(next);
    if (eased)
        moveUntil(now + kChangeMs);
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
    // A glowing cell reaches a cell past its edges, so take a row more
    // either side of what needs painting.
    const qsizetype first =
        std::max(qsizetype(0), qsizetype(event->rect().top() / pitch() - 1) * across);
    const qsizetype end =
        std::min(m_cells.size(), qsizetype(event->rect().bottom() / pitch() + 2) * across);
    // First every cell at its own size, then the glowing ones over their
    // neighbours.
    QList<qsizetype> glowing;
    for (qsizetype i = first; i < end; ++i) {
        const Cell &cell = m_cells[i];
        const QRect rect = cellRect(i);
        const qreal t = elapsed(cell.start, cell.length, now);
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
    const int side = m_theme.cellSide();
    for (const qsizetype i : glowing) {
        const Cell &cell = m_cells[i];
        painter.setOpacity(glowAt(elapsed(cell.start, cell.length, now)));
        painter.drawPixmap(cellRect(i).topLeft() - QPoint(side, side), glow(cell.to));
    }
    painter.setOpacity(1);
}

void DriveMapView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_map && capacity() != m_sortedFor)
        sortMap();
}

void DriveMapView::sortMap()
{
    m_cells.clear();
    m_sortedFor = capacity();
    if (m_map) {
        const QList<CellState> states = m_map->cells(m_sortedFor);
        m_cells.reserve(states.size());
        for (qsizetype i = 0; i < states.size(); ++i)
            m_cells.append({std::nullopt, states[i],
                            m_sweepStart + i * kSweepMs / states.size(), kArriveMs});
    }
    moveUntil(m_sweepStart + kSweepMs + kArriveMs);
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

const QPixmap &DriveMapView::tile(CellState state)
{
    QPixmap &pixmap = m_tiles[size_t(state)];
    const qreal ratio = devicePixelRatioF();
    const int side = m_theme.cellSide();
    if (pixmap.isNull() || pixmap.devicePixelRatio() != ratio
        || pixmap.deviceIndependentSize() != QSizeF(side, side))
        pixmap = paintedCell(m_theme, state, side, ratio);
    return pixmap;
}

const QPixmap &DriveMapView::glow(CellState state)
{
    QPixmap &pixmap = m_glows[size_t(state)];
    const qreal ratio = devicePixelRatioF();
    const int side = m_theme.cellSide();
    if (pixmap.isNull() || pixmap.devicePixelRatio() != ratio
        || pixmap.deviceIndependentSize() != QSizeF(3 * side, 3 * side))
        pixmap = paintedGlow(m_theme, state, side, ratio);
    return pixmap;
}

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
    for (const auto &[state, name] : entries) {
        auto *swatch = new QLabel;
        swatch->setPixmap(paintedCell(theme, state, theme.cellSide(), key->devicePixelRatioF()));
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
