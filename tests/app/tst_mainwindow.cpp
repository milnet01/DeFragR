// MainWindow and ReportView: the real window, offscreen, with its own
// buttons pressed. A fake engine (fakes/window.sh) lists drives and
// reports files whose names look like markup, and a stand-in pkexec
// (fakes/pkexec-pass, fakes/pkexec-refuse) answers for admin rights, so
// the test needs no root, no display and no real pkexec. DFR-0063; the
// defrag of a folder is docs/specs/DFR-0018-window-folder-defrag.md
// INV-11 and INV-12.
#include "FakeHold.h"
#include "model/Messages.h"
#include "ui/Languages.h"
#include "ui/DefragView.h"
#include "ui/DriveMapView.h"
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
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QTimer>

#include <memory>

using namespace defragr;

namespace {

constexpr int kWaitMs = 15000;
const QString kMarkupDrive = QStringLiteral("<img src=x>");
const QString kMarkupFile = QStringLiteral("<img src=x>.bin");
const QString kMarkupFs = QStringLiteral("<b>zfs</b>");
const QString kMarkupSkipped = QStringLiteral("<b>busy</b>.bin");

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

// True when `widget`'s tooltip reaches the eye with `literal` in it, tags
// and all. Qt draws a tooltip as markup when Qt::mightBeRichText says so.
bool tipShowsAsWritten(const QWidget *widget, const QString &literal)
{
    if (!widget)
        return false;
    QString shown = widget->toolTip();
    if (Qt::mightBeRichText(shown)) {
        QTextDocument markup;
        markup.setHtml(shown);
        shown = markup.toPlainText();
    }
    return shown.contains(literal);
}

}  // namespace

class TestMainWindow : public QObject {
    Q_OBJECT

    QDir m_fakes;
    QByteArray m_path;
    QTemporaryDir m_scratch;
    QString m_log;
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
    ui::DefragView *defragView() const { return m_window->findChild<ui::DefragView *>(); }

    // The command lines the fake engine has been started with.
    QStringList commandLines() const
    {
        QFile log(m_log);
        if (!log.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(log.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

    // Presses "Defrag now" with the folder dialog answering `folder`.
    // The box that follows, if one does, has its words noted in `shown`
    // and its button labelled `press` pressed.
    void defragNow(const QString &folder, const QString &press, QString *shown)
    {
        defragFrom(button(m_window.get(), QStringLiteral("Defrag now")), folder, press, shown);
    }

    // The same, from any button that asks for a folder to defrag.
    void defragFrom(QPushButton *start, const QString &folder, const QString &press,
                    QString *shown)
    {
        m_window->setFolderPicker([folder](QWidget *, const QString &) { return folder; });
        QTimer::singleShot(200, this, [=] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box)
                return;
            *shown = box->text();
            for (QAbstractButton *candidate : box->buttons())
                if (candidate->text().remove(QLatin1Char('&')) == press) {
                    candidate->click();
                    return;
                }
            box->reject();
        });
        start->click();
    }

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
        QVERIFY(m_scratch.isValid());
        m_log = m_scratch.filePath(QStringLiteral("log"));
        qputenv("DEFRAGR_TEST_LOG", QFile::encodeName(m_log));
    }

    void init() { QFile::remove(m_log); }

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

    // DFR-0018 INV-11: "Defrag now" starts no engine until a folder on
    // the drive is picked and the question about it is answered yes.
    void defragNeedsAFolderAndAYes()
    {
        useLauncher("pkexec-pass");
        openWindow("window.sh");
        selectRow(0);
        const QStringList listed = commandLines();
        QCOMPARE(listed, QStringList{QStringLiteral("probe")});

        // The folder dialog is cancelled: no box, no run.
        QString shown;
        defragNow({}, QStringLiteral("Defrag this folder"), &shown);
        QTest::qWait(300);
        QCOMPARE(shown, QString());
        QCOMPARE(pages()->currentIndex(), 0);

        // A folder on another drive, one whose name only starts the same.
        defragNow(QStringLiteral("/mnt/<img src=x>2/games"), QStringLiteral("OK"), &shown);
        QCOMPARE(shown, model::messages::folderNotOnDrive("/mnt/<img src=x>").what);
        QCOMPARE(pages()->currentIndex(), 0);

        // A folder on the drive, and the question answered no.
        shown.clear();
        defragNow(QStringLiteral("/mnt/<img src=x>/games"), QStringLiteral("Cancel"), &shown);
        QCOMPARE(shown, model::messages::confirmDefrag("/mnt/<img src=x>", "games").what);
        QCOMPARE(pages()->currentIndex(), 0);
        QTest::qWait(300);
        QCOMPARE(commandLines(), listed);
        QVERIFY(!defragView()->isRunning());
    }

    // S4, S7 and DFR-0018 INV-12: answered yes, the defrag page shows the
    // run, the drive's map, and at the end what happened to each file,
    // with every name as written. The report viewed first gave the map,
    // so only the defrag is run.
    void defragRunsAndShowsItsResult()
    {
        useLauncher("pkexec-pass");
        openWindow("window.sh");
        reportOnFirstDrive();
        button(reportView(), QStringLiteral("Back to drives"))->click();
        QFile::remove(m_log);

        FakeHold hold;
        QVERIFY(hold.isValid());
        QString shown;
        defragNow(QStringLiteral("/mnt/<img src=x>/games"), QStringLiteral("Defrag this folder"),
                  &shown);
        QCOMPARE(pages()->currentWidget(), defragView());
        auto *back = button(defragView(), QStringLiteral("Back to drives"));
        auto *map = defragView()->findChild<ui::DriveMapView *>();

        // Mid-run: the engine has moved one file's blocks, and waits.
        QVERIFY2(QTest::qWaitFor([&] { return hold.sent(); }, kWaitMs), "the defrag did not start");
        QVERIFY(!back->isEnabled());
        QVERIFY(showsAsWritten(label(defragView(), QStringLiteral("Defragging")), kMarkupDrive));
        QVERIFY(label(defragView(), QStringLiteral("Joining up the files")));
        // DFR-0099: the bar shows the work the engine counted, on the
        // view's next 1 s tick.
        auto *bar = defragView()->findChild<QProgressBar *>();
        QVERIFY(bar->isVisibleTo(m_window.get()));
        QVERIFY2(QTest::qWaitFor(
                     [&] { return label(defragView(), QStringLiteral("Finished with 0 of 3 files")); },
                     kWaitMs),
                 "the work was not shown");
        QCOMPARE(bar->maximum(), 100);
        QCOMPARE(bar->value(), 0);
        QVERIFY(map->isVisibleTo(m_window.get()));
        QVERIFY2(QTest::qWaitFor([&] { return map->markerCell() >= 0; }, kWaitMs),
                 "the move did not reach the map");
        QVERIFY(!defragView()->findChild<QTableView *>()->isVisibleTo(m_window.get()));
        const int mapTop = map->mapTo(defragView(), QPoint(0, 0)).y();

        // The end: the counts, the latest file, and the skipped file.
        QVERIFY(hold.release());
        QVERIFY2(QTest::qWaitFor([&] { return back->isEnabled(); }, kWaitMs),
                 "the defrag did not end");
        QCOMPARE(commandLines(), QStringList{QStringLiteral("defrag /mnt/<img src=x> games")});
        QVERIFY(label(defragView(), QStringLiteral("Done.")));
        QVERIFY(!bar->isVisibleTo(m_window.get()));
        auto *counts = label(defragView(), QStringLiteral("1 file moved into fewer pieces."));
        QVERIFY(counts);
        QVERIFY(counts->text().contains(QStringLiteral("1 file left as it was")));
        QVERIFY(counts->text().contains(QStringLiteral("1 file skipped.")));
        auto *latest = label(defragView(), QStringLiteral("Latest:"));
        QVERIFY(showsAsWritten(latest, kMarkupSkipped) || latest->text().contains(QChar(0x2026)));
        QVERIFY(tipShowsAsWritten(latest, kMarkupSkipped));
        auto *skipped = defragView()->findChild<QTableView *>();
        QVERIFY(skipped->isVisibleTo(m_window.get()));
        QCOMPARE(skipped->model()->rowCount(), 1);
        QVERIFY(skipped->model()->index(0, 0).data().toString().contains(kMarkupSkipped));
        engine::FileSkipped busy;
        busy.stage = engine::StageChanged;
        QCOMPARE(skipped->model()->index(0, 1).data().toString(),
                 model::messages::whySkipped(busy));
        // The map is the report's with the move made: its cells still
        // show a whole file where the blocks landed.
        QVERIFY(map->isVisibleTo(m_window.get()));
        // DFR-0110: more lines of counts, and the bar gone, move nothing.
        QTRY_COMPARE_WITH_TIMEOUT(map->mapTo(defragView(), QPoint(0, 0)).y(), mapTop, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(!map->isMoving(), kWaitMs);
        QVERIFY(map->cells().contains(model::CellState::Whole));
        QVERIFY(!map->cells().contains(model::CellState::Fragmented));

        back->click();
        QCOMPARE(pages()->currentIndex(), 0);
    }

    // S9: when the defrag is refused admin rights, the page says so and
    // shows no map: with no report viewed, the map's own run was refused.
    void refusedDefragIsExplained()
    {
        useLauncher("pkexec-refuse");
        openWindow("window.sh");
        selectRow(0);
        QString shown;
        defragNow(QStringLiteral("/mnt/<img src=x>"), QStringLiteral("Defrag this folder"), &shown);
        QCOMPARE(shown, model::messages::confirmDefrag("/mnt/<img src=x>", "").what);
        QCOMPARE(pages()->currentWidget(), defragView());
        auto *back = button(defragView(), QStringLiteral("Back to drives"));
        QVERIFY2(QTest::qWaitFor([&] { return back->isEnabled(); }, kWaitMs),
                 "the refused run did not end");
        engine::RunOutcome refused;
        refused.end = engine::RunEnd::AdminRefused;
        refused.enginePath = QFile::decodeName(qgetenv("DEFRAGR_ENGINE"));
        const model::Message expected = model::messages::forRun(refused);
        auto *said = label(defragView(), expected.what);
        QVERIFY2(said, "the refusal is not on the defrag page");
        QCOMPARE(said->text(), expected.what + QLatin1Char('\n') + expected.action);
        QVERIFY(!defragView()->findChild<ui::DriveMapView *>()->isVisibleTo(m_window.get()));
    }

    // S3: View report opens the report, which shows progress while it
    // runs and then the result, and Back returns to the drives.
    void reportRunsAndShowsItsResult()
    {
        FakeHold hold;
        QVERIFY(hold.isValid());
        useLauncher("pkexec-pass");
        openWindow("window.sh");
        selectRow(0);
        button(m_window.get(), QStringLiteral("View report"))->click();
        QCOMPARE(pages()->currentWidget(), reportView());

        // Mid-run: the engine has found a file and says it is half done,
        // then waits until the test lets it finish.
        QVERIFY2(QTest::qWaitFor([&] { return progress()->value() == 50; }, kWaitMs),
                 "no progress arrived");
        QVERIFY(progress()->isVisibleTo(m_window.get()));
        QCOMPARE(progress()->maximum(), 100);
        QVERIFY(!button(reportView(), QStringLiteral("Back to drives"))->isEnabled());
        // What it has found shows on the view's next 1 s tick (DFR-0061).
        QVERIFY2(QTest::qWaitFor(
                     [&] { return label(reportView(), QStringLiteral("Found 1 fragmented file so far.")); },
                     kWaitMs),
                 "the found file was not shown");
        QVERIFY(showsAsWritten(label(reportView(), QStringLiteral("Report on")), kMarkupDrive));
        QVERIFY(showsAsWritten(label(reportView(), QStringLiteral("Latest:")), kMarkupFile));
        QVERIFY(tipShowsAsWritten(label(reportView(), QStringLiteral("Latest:")), kMarkupFile));

        // The end: the figures replace the progress, and the file is listed.
        QVERIFY(hold.release());
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

    // DFR-0100: a report of a drive that can be defragged offers the
    // defrag itself, once it has loaded, and that defrag uses its map.
    void reportOffersADefrag()
    {
        FakeHold hold;
        QVERIFY(hold.isValid());
        useLauncher("pkexec-pass");
        openWindow("window.sh");
        selectRow(0);
        button(m_window.get(), QStringLiteral("View report"))->click();
        auto *defrag = button(reportView(), QStringLiteral("Defrag a folder"));
        QVERIFY(defrag);
        QVERIFY2(QTest::qWaitFor([&] { return progress()->value() == 50; }, kWaitMs),
                 "no progress arrived");
        QVERIFY(!defrag->isVisibleTo(m_window.get()));
        QVERIFY(hold.release());
        QVERIFY2(QTest::qWaitFor([&] { return defrag->isVisibleTo(m_window.get()); }, kWaitMs),
                 "the report did not offer a defrag");
        QFile::remove(m_log);

        QString shown;
        defragFrom(defrag, QStringLiteral("/mnt/<img src=x>/games"),
                   QStringLiteral("Defrag this folder"), &shown);
        QCOMPARE(shown, model::messages::confirmDefrag("/mnt/<img src=x>", "games").what);
        QCOMPARE(pages()->currentWidget(), defragView());
        QVERIFY2(QTest::qWaitFor(
                     [&] { return button(defragView(), QStringLiteral("Back to drives"))->isEnabled(); },
                     kWaitMs),
                 "the defrag did not end");
        QCOMPARE(commandLines(), QStringList{QStringLiteral("defrag /mnt/<img src=x> games")});
    }

    // DFR-0100: a drive that is report only gets no such offer.
    void reportOnlyDriveOffersNoDefrag()
    {
        useLauncher("pkexec-pass");
        openWindow("window.sh");
        selectRow(1);
        button(m_window.get(), QStringLiteral("View report"))->click();
        QVERIFY2(QTest::qWaitFor([&] { return button(reportView(), QStringLiteral("Back to drives"))->isEnabled(); },
                                 kWaitMs),
                 "the report did not end");
        QVERIFY(reportView()->findChild<ui::DriveMapView *>()->isVisibleTo(m_window.get()));
        QVERIFY(!button(reportView(), QStringLiteral("Defrag a folder"))->isVisibleTo(m_window.get()));
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
    // Each check runs before the window handles an event, so the probe
    // needs no pause.
    void refreshIsGreyedWhileListing()
    {
        useEngine("window.sh");
        m_window = std::make_unique<ui::MainWindow>(*m_theme);
        m_window->show();
        auto *refresh = button(m_window.get(), QStringLiteral("Refresh drives"));
        QVERIFY(refresh);
        // The window lists its drives as it opens.
        QVERIFY(!refresh->isEnabled());
        QVERIFY(label(m_window.get(), QStringLiteral("Listing drives…")));
        waitForDrives();
        QVERIFY(!label(m_window.get(), QStringLiteral("Listing drives…")));

        // And again when the user asks, with a drive picked: picked, it
        // could be reported on, but not while the list is replaced.
        selectRow(0);
        QVERIFY(button(m_window.get(), QStringLiteral("View report"))->isEnabled());
        refresh->click();
        QVERIFY(!refresh->isEnabled());
        QVERIFY(label(m_window.get(), QStringLiteral("Listing drives…")));
        QVERIFY(!button(m_window.get(), QStringLiteral("View report"))->isEnabled());
        waitForDrives();
        QVERIFY(refresh->isEnabled());
        QCOMPARE(driveTable()->model()->rowCount(), 2);
    }

private:
    QProgressBar *progress() const { return reportView()->findChild<QProgressBar *>(); }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
