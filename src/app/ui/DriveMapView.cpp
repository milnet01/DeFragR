#include "ui/DriveMapView.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>

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

// How far a cell's change has gone at `now`, eased: 0 not begun, 1 done.
qreal progress(qint64 start, qint64 now)
{
    const qreal linear = std::clamp(qreal(now - start) / DriveMapView::kEaseMs, 0.0, 1.0);
    return QEasingCurve(QEasingCurve::InOutCubic).valueForProgress(linear);
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
            shown = progress(old.start, now) >= 0.5 ? old.to : old.from;
        }
        next.append({shown, cells[i], now});
        eased = true;
    }
    m_cells = std::move(next);
    if (eased)
        moveUntil(now + kEaseMs);
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
    const qsizetype first = qsizetype(event->rect().top() / pitch()) * across;
    const qsizetype end =
        std::min(m_cells.size(), qsizetype(event->rect().bottom() / pitch() + 1) * across);
    for (qsizetype i = first; i < end; ++i) {
        const Cell &cell = m_cells[i];
        const QRect rect = cellRect(i);
        const qreal done = progress(cell.start, now);
        if (done < 1 && cell.from)
            painter.drawPixmap(rect.topLeft(), tile(*cell.from));
        if (done > 0) {
            painter.setOpacity(done);
            painter.drawPixmap(rect.topLeft(), tile(cell.to));
            painter.setOpacity(1);
        }
    }
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
            m_cells.append({std::nullopt, states[i], m_sweepStart + i * kSweepMs / states.size()});
    }
    moveUntil(m_sweepStart + kSweepMs + kEaseMs);
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
