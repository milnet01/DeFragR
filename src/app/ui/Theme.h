// Theme: every colour, map pattern and size the window uses, chosen in
// this one place (S10; docs/design.md § Who may call whom). A map cell's
// state shows as a pattern as well as a colour, so the map reads without
// colour. Text follows the desktop's size, never below a floor.
#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>

#include <array>

class QPainter;
class QRect;

namespace defragr::ui {

// What one map cell shows.
enum class CellState { Free, Filesystem, Whole, Fragmented };

class Theme {
public:
    // Desktop keeps the desktop's own window colours. The rest set their
    // own, and every one passes tst_theme's contrast checks.
    enum class Id { Desktop, DarkHighContrast, LightHighContrast, YellowOnBlack, Ocean, Ember, Aurora };
    static constexpr std::array kAll{Id::Desktop,     Id::DarkHighContrast, Id::LightHighContrast,
                                     Id::YellowOnBlack, Id::Ocean,          Id::Ember,
                                     Id::Aurora};

    // The smallest text, in points, and in pixels for a font sized in
    // pixels.
    static constexpr qreal kMinPointSize = 10;
    static constexpr int kMinPixelSize = 13;
    // The smallest map cell, in pixels: below it, stripes and dots blur
    // into each other.
    static constexpr int kMinCellSide = 8;

    // desktopPalette and desktopFont are the application's before any
    // theme is applied.
    Theme(Id id, const QPalette &desktopPalette, const QFont &desktopFont);

    Id id() const { return m_id; }

    // Sets the application's font, where the desktop's is below the
    // floor, and its colours, where the theme has its own. Call once,
    // before any window.
    void apply() const;

    QPalette palette() const { return m_palette; }
    QFont bodyFont() const { return m_body; }
    QFont headingFont() const;

    QColor mapBackground() const { return m_map.background; }
    QColor cellColour(CellState state) const;
    // A cell's side grows with the text, so a bigger desktop font gives a
    // bigger, easier map.
    int cellSide() const;
    int cellGap() const;
    // Paints the whole of one cell, background included: whole is solid,
    // fragmented is diagonal stripes, filesystem is four dots, free is a
    // faint outline.
    void paintCell(QPainter &painter, const QRect &cell, CellState state) const;

    // The map's colours. Public so the table of themes in Theme.cpp can
    // name them.
    struct MapColours {
        QColor background;
        QColor whole;
        QColor fragmented;
        QColor filesystem;
        // Decoration only: free space is read from the absence of a
        // pattern, so this outline may be faint.
        QColor free;
    };

private:
    Id m_id;
    QPalette m_palette;
    bool m_ownPalette = false;
    QFont m_body;
    bool m_raisedFont = false;
    MapColours m_map;
};

}  // namespace defragr::ui
