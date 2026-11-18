// MainWindow and ReportView: the real window, offscreen, with its own
// buttons pressed. A fake engine (fakes/window.sh) lists drives and
// reports files whose names look like markup, and a stand-in pkexec
// (fakes/pkexec-pass, fakes/pkexec-refuse) answers for admin rights, so
// the test needs no root, no display and no real pkexec. DFR-0063.
#include "model/Messages.h"
#include "ui/Languages.h"
#include "ui/MainWindow.h"
#include "ui/ReportView.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QTest>
#include <QTimer>

#include <memory>

using namespace defragr;

namespace {

constexpr int kWaitMs = 15000;
const QString kMarkupDrive = QStringLiteral("<img src=x>");
const QString kMarkupFile = QStringLiteral("<img src=x>.bin");
const QString kMarkupFs = QStringLiteral("<b>zfs</b>");

QPushButton *button(QWidget *window, const QString &text)
{
    for (auto *candidate : window->findChildren<QPushButton *>())
        if (candidate->text() == text)
            return candidate;
    return nullptr;
}

// The label whose text starts so; the labels have no names to look up.
QLabel *label(QWidget *window, const QString &start)
{
    for (auto *candidate : window->findChildren<QLabel *>())
        if (candidate->text().startsWith(start))
            return candidate;
    return nullptr;
}

// True when `shown` reaches the eye as `literal`, tags and all: the label
// is plain text, or its text is not one Qt would read as markup.
bool showsAsWritten(const QLabel *shown, const QString &literal)
{
    if (!shown || !shown->text().contains(literal))
        return false;
    return shown->textFormat() == Qt::PlainText
           || (shown->textFormat() == Qt::AutoText && !Qt::mightBeRichText(shown->text()));
}

}  // namespace

class TestMainWindow : public QObject {
    Q_OBJECT

    QDir m_fakes;
    QByteArray m_path;
    // English's wording, whatever this machine's language.
    std::unique_ptr<ui::Languages> m_english;
    std::unique_ptr<ui::Theme> m_theme;
    std::unique_ptr<ui::MainWindow> m_window;

    QString fake(const char *name) const { return m_fakes.filePath(QLatin1String(name)); }
    void useEngine(const char *name) { qputenv("DEFRAGR_ENGINE", QFile::encodeName(fake(name))); }
    // The window finds its launcher, pkexec, on PATH.
    void useLauncher(const char *folder)
    {
        qputenv("PATH", QFile::encodeName(fake(folder)) + ':' + m_path);
    }

    QTableView *driveTable() const { return m_window->findChild<QStackedWidget *>()->widget(0)->findChild<QTableView *>(); }
    QStackedWidget *pages() const { return m_window->findChild<QStackedWidget *>(); }
    ui::ReportView *reportView() const { return m_window->findChild<ui::ReportView *>(); }

    // A window that has listed its drives from `engine`.
    void openWindow(const char *engine)
    {
        useEngine(engine);
        m_window = std::make_unique<ui::MainWindow>(*m_theme);
        m_window->show();
        waitForDrives();
    }

    void waitForDrives()
    {
        QVERIFY2(QTest::qWaitFor([&] { return button(m_window.get(), QStringLiteral("Refresh drives"))->isEnabled(); },
                                 kWaitMs),
                 "the drives did not list");
    }

    void selectRow(int row)
    {
        driveTable()->selectRow(row);
        QVERIFY(driveTable()->selectionModel()->isRowSelected(row));
    }

    // Presses "View report" on the first drive and waits for the run to
    // end: Back to drives comes back on.
    void reportOnFirstDrive()
    {
        selectRow(0);
        button(m_window.get(), QStringLiteral("View report"))->click();
        QCOMPARE(pages()->currentWidget(), reportView());
        QVERIFY2(QTest::qWaitFor([&] { return button(reportView(), QStringLiteral("Back to drives"))->isEnabled(); },
                                 kWaitMs),
                 "the report did not end");
    }

private slots:
    void initTestCase()
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        m_english = std::make_unique<ui::Languages>(QLocale(QLocale::English));
        m_theme = std::make_unique<ui::Theme>(ui::Theme::Id::Desktop, QApplication::palette(),
                                              QApplication::font());
        const QByteArray fakes = qgetenv("DEFRAGR_FAKES");
        QVERIFY2(!fakes.isEmpty(), "DEFRAGR_FAKES names the folder of fake engines");
        m_fakes = QDir(QFile::decodeName(fakes));
        m_path = qgetenv("PATH");
    }

    void cleanup()
    {
        m_window.reset();
        qputenv("PATH", m_path);
        qunsetenv("DEFRAGR_ENGINE");
    }

    // S1 and S2: nothing is chosen until a drive is picked, then each
    // button follows that drive, and names show as written.
    void drivesAndTheirActions()
    {
        openWindow("window.sh");
        QAbstractItemModel *drives = driveTable()->model();
        QCOMPARE(drives->rowCount(), 2);
        QVERIFY(drives->index(0, 0).data().toString().contains(kMarkupDrive));
        QVERIFY(drives->index(1, 2).data().toString().contains(kMarkupFs));

        auto *note = label(m_window.get(), QStringLiteral("Pick a drive to"));
        QVERIFY(note);
        QVERIFY(!button(m_window.get(), QStringLiteral("View report"))->isEnabled());
        QVERIFY(!button(m_window.get(), QStringLiteral("Defrag now"))->isEnabled());

        // An ext4 drive: both, and the note says so.
        selectRow(0);
        QVERIFY(button(m_window.get(), QStringLiteral("View report"))->isEnabled());
        QVERIFY(button(m_window.get(), QStringLiteral("Defrag now"))->isEnabled());
        QCOMPARE(note->text(), model::messages::defragAllowed());

        // A drive of another kind: a report only, and the note says why,
        // with the filesystem's name as written and not as markup.
        selectRow(1);
        QVERIFY(button(m_window.get(), QStringLiteral("View report"))->isEnabled());
        QVERIFY(!button(m_window.get(), QStringLiteral("Defrag now"))->isEnabled());
        QVERIFY2(showsAsWritten(note, kMarkupFs), qPrintable(note->text()));
    }

    // "Defrag now" is not built yet; it says so in a box.
    void defragNowSaysItComesLater()
    {
        openWindow("window.sh");
        selectRow(0);
        QString shown;
        QTimer::singleShot(200, this, [&] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                shown = box->text();
                box->accept();
            }
        });
        button(m_window.get(), QStringLiteral("Defrag now"))->click();
        QCOMPARE(shown, model::messages::defragComesLater());
        QCOMPARE(pages()->currentIndex(), 0);
    }

    // S3: View report opens the report, which shows progress while it
    // runs and then the result, and Back returns to the drives.
    void reportRunsAndShowsItsResult()
    {
        useLauncher("pkexec-pass");
        openWindow("window.sh");
        selectRow(0);
        button(m_window.get(), QStringLiteral("View report"))->click();
        QCOMPARE(pages()->currentWidget(), reportView());

        // Mid-run: the engine has found a file and says it is half done,
        // then waits.
        QVERIFY2(QTest::qWaitFor([&] { return progress()->value() == 50; }, kWaitMs),
                 "no progress arrived");
        QVERIFY(progress()->isVisibleTo(m_window.get()));
        QCOMPARE(progress()->maximum(), 100);
        QVERIFY(!button(reportView(), QStringLiteral("Back to drives"))->isEnabled());
        // What it has found shows on the view's next 1 s tick (DFR-0061),
        // well inside the engine's 2 s wait.
        QVERIFY2(QTest::qWaitFor(
                     [&] { return label(reportView(), QStringLiteral("Fragmented files found so far: 1")); },
                     1500),
                 "the found file was not shown within a tick");
        QVERIFY(showsAsWritten(label(reportView(), QStringLiteral("Report on")), kMarkupDrive));
        QVERIFY(showsAsWritten(label(reportView(), QStringLiteral("Latest:")), kMarkupFile));

        // The end: the figures replace the progress, and the file is listed.
        QVERIFY2(QTest::qWaitFor([&] { return button(reportView(), QStringLiteral("Back to drives"))->isEnabled(); },
                                 kWaitMs),
                 "the report did not end");
        QVERIFY(!progress()->isVisibleTo(m_window.get()));
        auto *files = reportView()->findChild<QTableView *>();
        QVERIFY(files->isVisibleTo(m_window.get()));
        QCOMPARE(files->model()->rowCount(), 1);
        QVERIFY(files->model()->index(0, 0).data().toString().contains(kMarkupFile)
                || files->model()->index(0, 1).data().toString().contains(kMarkupFile)
                || files->model()->index(0, 2).data().toString().contains(kMarkupFile));
        QVERIFY(showsAsWritten(label(reportView(), QStringLiteral("Report on")), kMarkupDrive));

        button(reportView(), QStringLiteral("Back to drives"))->click();
        QCOMPARE(pages()->currentIndex(), 0);
    }

    // S9: when pkexec says no, the report page says admin rights were
    // refused, and does not show a result.
    void refusedAdminRightsAreExplained()
    {
        useLauncher("pkexec-refuse");
        openWindow("window.sh");
        reportOnFirstDrive();

        engine::RunOutcome refused;
        refused.end = engine::RunEnd::AdminRefused;
        refused.enginePath = QFile::decodeName(qgetenv("DEFRAGR_ENGINE"));
        const model::Message expected = model::messages::forRun(refused);
        QVERIFY(!expected.what.isEmpty());
        auto *shown = label(reportView(), expected.what);
        QVERIFY2(shown, "the refusal is not on the report page");
        QCOMPARE(shown->text(), expected.what + QLatin1Char('\n') + expected.action);
        QVERIFY(!progress()->isVisibleTo(m_window.get()));
        QVERIFY(!reportView()->findChild<QTableView *>()->isVisibleTo(m_window.get()));
    }

    // While a probe runs, Refresh is greyed out and the window says so.
    void refreshIsGreyedWhileListing()
    {
        useEngine("slow-probe.sh");
        m_window = std::make_unique<ui::MainWindow>(*m_theme);
        m_window->show();
        auto *refresh = button(m_window.get(), QStringLiteral("Refresh drives"));
        QVERIFY(refresh);
        // The window lists its drives as it opens.
        QVERIFY(!refresh->isEnabled());
        QVERIFY(label(m_window.get(), QStringLiteral("Listing drives…")));
        waitForDrives();
        QVERIFY(!label(m_window.get(), QStringLiteral("Listing drives…")));

        // And again when the user asks.
        refresh->click();
        QVERIFY(!refresh->isEnabled());
        QVERIFY(label(m_window.get(), QStringLiteral("Listing drives…")));
        QVERIFY(!button(m_window.get(), QStringLiteral("View report"))->isEnabled());
        waitForDrives();
        QVERIFY(refresh->isEnabled());
        QCOMPARE(driveTable()->model()->rowCount(), 1);
    }

private:
    QProgressBar *progress() const { return reportView()->findChild<QProgressBar *>(); }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
