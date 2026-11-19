// Placement: the window's size and position kept between runs, offscreen.
// What needs KWin itself is checked here only as far as it can be without
// one: the script it is sent and the journal line it answers with. The
// test never asks the desktop's KWin for anything: offscreen, Placement
// moves the window itself. DFR-0109.
#include "ui/Placement.h"

#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>

#include <memory>

using namespace defragr;

namespace {

constexpr QSize kStartSize{400, 250};

}  // namespace

class TestPlacement : public QObject {
    Q_OBJECT

public:
    // Before the application exists, so no test can reach a real desktop.
    static void initMain() { qputenv("QT_QPA_PLATFORM", "offscreen"); }

private:
    QTemporaryDir m_scratch;

    std::unique_ptr<QSettings> settings() const
    {
        return std::make_unique<QSettings>(m_scratch.filePath(QStringLiteral("placement.conf")),
                                           QSettings::IniFormat);
    }

    // A window as main() makes one: its start size, then its Placement.
    std::unique_ptr<QWidget> openWindow() const
    {
        auto window = std::make_unique<QWidget>();
        window->resize(kStartSize);
        new ui::Placement(*window, settings());
        return window;
    }

    QSize room() const { return QGuiApplication::primaryScreen()->availableGeometry().size(); }

private slots:
    void initTestCase()
    {
        QVERIFY(m_scratch.isValid());
        // The user's own settings file is never this test's to read or write.
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_scratch.filePath(QStringLiteral("config"))));
    }

    void init() { QFile::remove(m_scratch.filePath(QStringLiteral("placement.conf"))); }

    // The real settings live in the user's own config folder.
    void settingsAreTheUsersOwn()
    {
        QCOMPARE(ui::Placement::userSettings()->fileName(),
                 m_scratch.filePath(QStringLiteral("config/DeFragR/DeFragR.conf")));
    }

    void firstRunKeepsTheStartSize()
    {
        const auto window = openWindow();
        QCOMPARE(window->size(), kStartSize);
    }

    void sizeAndPositionComeBack()
    {
        {
            const auto window = openWindow();
            window->show();
            QVERIFY(QTest::qWaitForWindowExposed(window.get()));
            window->resize(520, 300);
            window->move(40, 30);
            window->close();
        }
        QCOMPARE(settings()->value(QStringLiteral("window/size")).toSize(), QSize(520, 300));

        const auto again = openWindow();
        QCOMPARE(again->size(), QSize(520, 300));
        QCOMPARE(again->pos(), QPoint(40, 30));
    }

    // A size saved on a bigger screen, or a broken file, never makes a
    // window the user cannot use.
    void badSavedSizes_data()
    {
        QTest::addColumn<QVariant>("saved");
        QTest::addColumn<QSize>("expected");
        QTest::newRow("larger than the screen") << QVariant(QSize(100000, 90000)) << room();
        QTest::newRow("words") << QVariant(QStringLiteral("banana")) << kStartSize;
        QTest::newRow("negative") << QVariant(QSize(-5, 300)) << kStartSize;
        QTest::newRow("empty") << QVariant(QSize(0, 0)) << kStartSize;
    }

    void badSavedSizes()
    {
        QFETCH(QVariant, saved);
        QFETCH(QSize, expected);
        settings()->setValue(QStringLiteral("window/size"), saved);
        const auto window = openWindow();
        QCOMPARE(window->size(), expected);
    }

    // A position off every screen comes back onto one.
    void positionOffScreenIsPulledBack()
    {
        settings()->setValue(QStringLiteral("window/position"), QPoint(50000, -900));
        const auto window = openWindow();
        const QRect area = QGuiApplication::primaryScreen()->availableGeometry();
        QVERIFY2(area.contains(QRect(window->pos(), window->size())),
                 qPrintable(QStringLiteral("the window is at %1,%2")
                                .arg(window->pos().x())
                                .arg(window->pos().y())));
    }

    void journalAnswer_data()
    {
        QTest::addColumn<QString>("journal");
        QTest::addColumn<bool>("found");
        QTest::addColumn<QPoint>("at");
        const QString other = QStringLiteral("kwin_wayland: something else\n");
        QTest::newRow("among other lines")
            << other + QStringLiteral("js: m1 120 45\n") + other << true << QPoint(120, 45);
        QTest::newRow("left of the main screen")
            << QStringLiteral("m1 -1920 0") << true << QPoint(-1920, 0);
        QTest::newRow("no answer") << other << false << QPoint();
        QTest::newRow("another marker") << QStringLiteral("m2 1 2") << false << QPoint();
        QTest::newRow("not numbers") << QStringLiteral("m1 left top") << false << QPoint();
        QTest::newRow("one number") << QStringLiteral("m1 7") << false << QPoint();
    }

    void journalAnswer()
    {
        QFETCH(QString, journal);
        QFETCH(bool, found);
        QFETCH(QPoint, at);
        const std::optional<QPoint> answer =
            ui::Placement::frameFromJournal(journal, QStringLiteral("m1"));
        QCOMPARE(answer.has_value(), found);
        if (found)
            QCOMPARE(*answer, at);
    }

    // Both scripts act on DeFragR's own main window: the one with its
    // process number that is not a dialog.
    void scriptsPickTheMainWindow()
    {
        const QString read = ui::Placement::readFrameScript(4242, QStringLiteral("m1"));
        const QString move = ui::Placement::moveFrameScript(4242, QPoint(-30, 70));
        for (const QString &script : {read, move}) {
            QVERIFY(script.contains(QStringLiteral("c.pid !== 4242 || c.transientFor")));
        }
        QVERIFY(read.contains(QStringLiteral("print(\"m1\", Math.round(")));
        QVERIFY(move.contains(QStringLiteral("var x = -30;")));
        QVERIFY(move.contains(QStringLiteral("var y = 70;")));
        QVERIFY(move.contains(QStringLiteral("KWin.PlacementArea")));
    }
};

QTEST_MAIN(TestPlacement)
#include "tst_placement.moc"
