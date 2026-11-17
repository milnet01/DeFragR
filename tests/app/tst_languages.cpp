// Languages: the window speaks the desktop's language, English where
// DeFragR has no translation, and Hebrew lays it out right to left (S13).
// fixtures/defragr_he.ts holds one Hebrew sentence, so this test does not
// wait for the real translations (DFR-0067).
#include "model/Messages.h"
#include "model/Report.h"
#include "ui/Languages.h"

#include <QGuiApplication>
#include <QTest>

using namespace defragr;

class TestLanguages : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The expected wording below is written in this locale's numbers.
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void cleanup() { QGuiApplication::setLayoutDirection(Qt::LeftToRight); }

    // First, so the tests after it show that Hebrew leaves with its
    // Languages.
    void hebrewReadsRightToLeft()
    {
        // Without Qt's own Hebrew, which would turn the layout round too.
        const ui::Languages languages(QLocale(QLocale::Hebrew, QLocale::Israel),
                                      QStringLiteral(":/no-such-folder"));
        QCOMPARE(languages.chosen().language(), QLocale::Hebrew);
        QCOMPARE(model::messages::refreshButton(), QStringLiteral("רענן כוננים"));
        // Installing a translation posts a language change, on which Qt
        // may set the direction again. It must stay right to left.
        QCoreApplication::processEvents();
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::RightToLeft);
    }

    void noTranslationMeansEnglish()
    {
        const ui::Languages languages(QLocale(QLocale::German, QLocale::Germany));
        QCOMPARE(languages.chosen().language(), QLocale::English);
        QCOMPARE(model::messages::refreshButton(), QStringLiteral("Refresh drives"));
        // English's plural forms are in, so it never shows "file(s)".
        QCOMPARE(model::progressLine(1, 2),
                 QStringLiteral("Checked 1 of about 2 files and folders (50%)."));
        QCoreApplication::processEvents();
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::LeftToRight);
    }

    void englishIsEnglish()
    {
        const ui::Languages languages(QLocale(QLocale::English, QLocale::UnitedKingdom));
        QCOMPARE(languages.chosen().language(), QLocale::English);
        QCOMPARE(model::progressLine(1, 1),
                 QStringLiteral("Checked 1 of about 1 file or folder (99%)."));
    }
};

QTEST_MAIN(TestLanguages)
#include "tst_languages.moc"
