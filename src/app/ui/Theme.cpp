#include "ui/Theme.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace defragr::ui {

namespace {

struct WindowColours {
    QColor window;
    QColor text;
    QColor base;
    QColor alternateBase;
    QColor button;
    QColor highlight;
    QColor highlightedText;
    // Disabled and placeholder text: dimmer than text, still readable.
    QColor dim;
};

struct Colours {
    WindowColours window;
    Theme::MapColours map;
};

// The block colours of the high-contrast themes are from the Okabe-Ito
// set, which colour-blind eyes still tell apart.
const Colours kDarkHighContrast{
    {"#000000", "#ffffff", "#0a0a0a", "#1c1c1c", "#1c1c1c", "#ffd400", "#000000", "#c0c0c0"},
    {"#000000", "#56b4e9", "#e69f00", "#b0b0b0", "#4a4a4a"}};
const Colours kLightHighContrast{
    {"#ffffff", "#000000", "#ffffff", "#ececec", "#e6e6e6", "#003c96", "#ffffff", "#505050"},
    {"#ffffff", "#0050b0", "#a83800", "#505050", "#b8b8b8"}};
const Colours kYellowOnBlack{
    {"#000000", "#ffff00", "#000000", "#1a1a00", "#1a1a00", "#ffff00", "#000000", "#c8c800"},
    {"#000000", "#ffff00", "#ffffff", "#a0a000", "#505000"}};
// The rest are for watching: calm grounds and glowing blocks.
const Colours kOcean{
    {"#071a2c", "#e8f4ff", "#0b2238", "#10304c", "#12304c", "#4fd1c5", "#04121f", "#a8c0d8"},
    {"#04121f", "#4fd1c5", "#ff8a65", "#8fa9c4", "#1f3a55"}};
const Colours kEmber{
    {"#17110e", "#fff2e6", "#1e1612", "#2b201a", "#2b201a", "#ffb347", "#17110e", "#c8b4a4"},
    {"#120c09", "#ffb347", "#e866b8", "#a89080", "#3a2c24"}};
const Colours kAurora{
    {"#0b0e1a", "#eaf7f0", "#10142a", "#1a2040", "#1a2040", "#5cf2a6", "#0b0e1a", "#b0bcd8"},
    {"#070914", "#5cf2a6", "#c08cff", "#7f8fb0", "#23294a"}};

const Colours &coloursOf(Theme::Id id)
{
    switch (id) {
    case Theme::Id::Desktop:  // Never asked for; listed so every Id is covered.
    case Theme::Id::DarkHighContrast:
        return kDarkHighContrast;
    case Theme::Id::LightHighContrast:
        return kLightHighContrast;
    case Theme::Id::YellowOnBlack:
        return kYellowOnBlack;
    case Theme::Id::Ocean:
        return kOcean;
    case Theme::Id::Ember:
        return kEmber;
    case Theme::Id::Aurora:
        return kAurora;
    }
    return kDarkHighContrast;
}

bool isDark(const QColor &colour) { return colour.lightness() < 128; }

QPalette paletteOf(const WindowColours &c)
{
    // Derives the frame shades (light, mid, dark, shadow) from these two.
    QPalette palette(c.button, c.window);
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const bool off = group == QPalette::Disabled;
        palette.setColor(group, QPalette::WindowText, off ? c.dim : c.text);
        palette.setColor(group, QPalette::Text, off ? c.dim : c.text);
        palette.setColor(group, QPalette::ButtonText, off ? c.dim : c.text);
        palette.setColor(group, QPalette::Base, c.base);
        palette.setColor(group, QPalette::AlternateBase, c.alternateBase);
        palette.setColor(group, QPalette::Highlight, c.highlight);
        palette.setColor(group, QPalette::HighlightedText, c.highlightedText);
        palette.setColor(group, QPalette::ToolTipBase, c.base);
        palette.setColor(group, QPalette::ToolTipText, c.text);
        palette.setColor(group, QPalette::PlaceholderText, c.dim);
        palette.setColor(group, QPalette::BrightText, c.highlight);
        palette.setColor(group, QPalette::Link, c.highlight);
    }
    return palette;
}

}  // namespace

Theme::Theme(Id id, const QPalette &desktopPalette, const QFont &desktopFont)
    : m_id(id), m_palette(desktopPalette), m_body(desktopFont)
{
    if (id == Id::Desktop) {
        // The map keeps high contrast on a desktop of either kind.
        const bool dark = isDark(desktopPalette.color(QPalette::Window));
        m_map = (dark ? kDarkHighContrast : kLightHighContrast).map;
    } else {
        m_map = coloursOf(id).map;
        m_palette = paletteOf(coloursOf(id).window);
        m_ownPalette = true;
    }

    // A font is sized in points or in pixels, never both; the other reads
    // -1.
    if (m_body.pointSizeF() > 0 && m_body.pointSizeF() < kMinPointSize) {
        m_body.setPointSizeF(kMinPointSize);
        m_raisedFont = true;
    } else if (m_body.pointSizeF() <= 0 && m_body.pixelSize() < kMinPixelSize) {
        m_body.setPixelSize(kMinPixelSize);
        m_raisedFont = true;
    }
}

void Theme::apply() const
{
    // Left alone otherwise, so the desktop can still change them while
    // the window is open.
    if (m_raisedFont)
        QGuiApplication::setFont(m_body);
    if (m_ownPalette)
        QGuiApplication::setPalette(m_palette);
}

QFont Theme::headingFont() const
{
    QFont heading = m_body;
    if (heading.pointSizeF() > 0)
        heading.setPointSizeF(heading.pointSizeF() * 1.25);
    else
        heading.setPixelSize(int(std::lround(heading.pixelSize() * 1.25)));
    heading.setBold(true);
    return heading;
}

QColor Theme::cellColour(CellState state) const
{
    switch (state) {
    case CellState::Free:
        return m_map.free;
    case CellState::Filesystem:
        return m_map.filesystem;
    case CellState::Whole:
        return m_map.whole;
    case CellState::Fragmented:
        return m_map.fragmented;
    }
    return m_map.free;
}

QGradientStops Theme::glowHalo(CellState state) const
{
    QColor colour = cellColour(state);
    if (isDark(m_map.background))
        colour = colour.lighter(150);
    QColor half = colour;
    half.setAlpha(110);
    QColor none = colour;
    none.setAlpha(0);
    return {{0, colour}, {0.45, half}, {1, none}};
}

QColor Theme::glowWash(CellState state) const
{
    return isDark(m_map.background) ? QColor(Qt::white) : cellColour(state);
}

int Theme::cellSide() const
{
    // About two thirds of a line of text.
    return std::max(kMinCellSide, QFontMetrics(m_body).height() * 2 / 3);
}

int Theme::cellGap() const { return std::max(1, cellSide() / 10); }

void Theme::paintCell(QPainter &painter, const QRect &cell, CellState state) const
{
    painter.save();
    painter.fillRect(cell, m_map.background);
    const QColor ink = cellColour(state);
    const qreal side = std::min(cell.width(), cell.height());

    switch (state) {
    case CellState::Whole:
        painter.fillRect(cell, ink);
        break;
    case CellState::Fragmented: {
        // Three stripes rising left to right, cut off at the cell's edge.
        // Crisp: smoothed edges fill a small cell's gaps until it reads as
        // solid.
        painter.setClipRect(cell);
        painter.setPen(QPen(ink, std::max(1.0, side / 6), Qt::SolidLine, Qt::FlatCap));
        const qreal step = side / 2;
        for (int i = -1; i <= 1; ++i) {
            const qreal shift = i * step;
            painter.drawLine(QPointF(cell.left() + shift, cell.bottom() + 1),
                             QPointF(cell.right() + 1 + shift, cell.top()));
        }
        break;
    }
    case CellState::Filesystem: {
        const int dot = std::max(2, int(side / 4));
        for (const qreal fx : {0.25, 0.75})
            for (const qreal fy : {0.25, 0.75})
                painter.fillRect(int(cell.left() + fx * cell.width() - dot / 2.0),
                                 int(cell.top() + fy * cell.height() - dot / 2.0), dot, dot, ink);
        break;
    }
    case CellState::Free:
        painter.setPen(QPen(ink, 1));
        painter.drawRect(cell.adjusted(0, 0, -1, -1));
        break;
    }
    painter.restore();
}

}  // namespace defragr::ui
