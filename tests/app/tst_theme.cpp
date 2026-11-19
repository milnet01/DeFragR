// Theme: every theme is high contrast, the map's patterns tell its cells
// apart with no colour at all, and text is never below the floor (S10).
// Contrast is WCAG 2's ratio: 7 for text, 4.5 for dimmed text and the
// selection, 3 for a map pattern against its ground.
#include "ui/Theme.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTest>

#include <cmath>

using defragr::ui::CellState;
using defragr::ui::Theme;

namespace {

constexpr std::array kStates{CellState::Free, CellState::Filesystem, CellState::Whole,
                             CellState::LargePieces, CellState::Fragmented};

double luminance(const QColor &colour)
{
    const auto channel = [](double c) {
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(colour.redF()) + 0.7152 * channel(colour.greenF())
           + 0.0722 * channel(colour.blueF());
}

double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QPalette darkDesktop()
{
    return QPalette(QColor("#31363b"), QColor("#202326"));
}

QPalette lightDesktop()
{
    return QPalette(QColor("#fcfcfc"), QColor("#eff0f1"));
}

QFont pointFont(qreal points)
{
    QFont font(QStringLiteral("Sans"));
    font.setPointSizeF(points);
    return font;
}

Theme themeOf(Theme::Id id)
{
    return Theme(id, darkDesktop(), pointFont(10));
}

// The pixels the pattern inks: those differing from the map's ground.
// Colour plays no part, which is the point.
QImage inkOf(const Theme &theme, CellState state, int side)
{
    QImage image(side, side, QImage::Format_ARGB32);
    QPainter painter(&image);
    theme.paintCell(painter, QRect(0, 0, side, side), state);
    painter.end();
    QImage ink(side, side, QImage::Format_Mono);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x)
            ink.setPixel(x, y, image.pixelColor(x, y) != theme.mapBackground() ? 1 : 0);
    return ink;
}

double differingShare(const QImage &a, const QImage &b)
{
    int differing = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            differing += a.pixelIndex(x, y) != b.pixelIndex(x, y);
    return double(differing) / (a.width() * a.height());
}

}  // namespace

class TestTheme : public QObject {
    Q_OBJECT

private slots:
    void contrast_data()
    {
        QTest::addColumn<Theme::Id>("id");
        for (const Theme::Id id : Theme::kAll)
            QTest::newRow(QByteArray::number(int(id))) << id;
    }

    // Desktop's window colours are the desktop's, so only its map is
    // checked.
    void contrast()
    {
        QFETCH(Theme::Id, id);
        const Theme theme = themeOf(id);
        for (const CellState state : {CellState::Filesystem, CellState::Whole, CellState::Fragmented})
            QVERIFY2(contrastRatio(theme.cellColour(state), theme.mapBackground()) >= 3,
                     qPrintable(QString::number(int(state))));
        if (id == Theme::Id::Desktop)
            return;
        const QPalette p = theme.palette();
        QVERIFY(contrastRatio(p.color(QPalette::WindowText), p.color(QPalette::Window)) >= 7);
        QVERIFY(contrastRatio(p.color(QPalette::Text), p.color(QPalette::Base)) >= 7);
        QVERIFY(contrastRatio(p.color(QPalette::Text), p.color(QPalette::AlternateBase)) >= 7);
        QVERIFY(contrastRatio(p.color(QPalette::ButtonText), p.color(QPalette::Button)) >= 7);
        QVERIFY(contrastRatio(p.color(QPalette::HighlightedText), p.color(QPalette::Highlight))
                >= 4.5);
        // A disabled button and its reason stay readable (S12).
        QVERIFY(contrastRatio(p.color(QPalette::Disabled, QPalette::ButtonText),
                                 p.color(QPalette::Disabled, QPalette::Button))
                >= 4.5);
        QVERIFY(contrastRatio(p.color(QPalette::Disabled, QPalette::WindowText),
                                 p.color(QPalette::Disabled, QPalette::Window))
                >= 4.5);
    }

    // Each pair of patterns differs in at least a sixth of the cell's
    // pixels, at the smallest cell and a large one.
    void patternsReadWithoutColour_data() { contrast_data(); }
    void patternsReadWithoutColour()
    {
        QFETCH(Theme::Id, id);
        const Theme theme = themeOf(id);
        for (const int side : {Theme::kMinCellSide, 24}) {
            for (std::size_t i = 0; i < kStates.size(); ++i) {
                for (std::size_t j = i + 1; j < kStates.size(); ++j) {
                    const double share = differingShare(inkOf(theme, kStates[i], side),
                                                        inkOf(theme, kStates[j], side));
                    QVERIFY2(share >= 1.0 / 6,
                             qPrintable(QStringLiteral("side %1, states %2 and %3: %4")
                                            .arg(side).arg(int(kStates[i])).arg(int(kStates[j]))
                                            .arg(share)));
                }
            }
        }
    }

    void desktopMapFollowsDesktopLightness()
    {
        const Theme onDark(Theme::Id::Desktop, darkDesktop(), pointFont(10));
        QCOMPARE(onDark.mapBackground(), themeOf(Theme::Id::DarkHighContrast).mapBackground());
        QCOMPARE(onDark.palette(), darkDesktop());
        const Theme onLight(Theme::Id::Desktop, lightDesktop(), pointFont(10));
        QCOMPARE(onLight.mapBackground(),
                 Theme(Theme::Id::LightHighContrast, lightDesktop(), pointFont(10)).mapBackground());
    }

    void textNeverBelowTheFloor()
    {
        QCOMPARE(Theme(Theme::Id::Desktop, darkDesktop(), pointFont(7)).bodyFont().pointSizeF(),
                 Theme::kMinPointSize);
        QCOMPARE(Theme(Theme::Id::Desktop, darkDesktop(), pointFont(14)).bodyFont().pointSizeF(),
                 14.0);
        QFont pixels(QStringLiteral("Sans"));
        pixels.setPixelSize(9);
        QCOMPARE(Theme(Theme::Id::Desktop, darkDesktop(), pixels).bodyFont().pixelSize(),
                 Theme::kMinPixelSize);
        // A heading is a step larger than the text.
        const Theme theme = themeOf(Theme::Id::Desktop);
        QVERIFY(theme.headingFont().pointSizeF() > theme.bodyFont().pointSizeF());
        QVERIFY(theme.headingFont().bold());
    }

    // A bigger desktop font gives bigger cells, and a tiny one still
    // gives the smallest that reads.
    void cellsGrowWithText()
    {
        const Theme small(Theme::Id::Desktop, darkDesktop(), pointFont(10));
        const Theme large(Theme::Id::Desktop, darkDesktop(), pointFont(20));
        QVERIFY(large.cellSide() > small.cellSide());
        QVERIFY(small.cellSide() >= Theme::kMinCellSide);
        QVERIFY(small.cellGap() >= 1);
    }
};

QTEST_MAIN(TestTheme)
#include "tst_theme.moc"
