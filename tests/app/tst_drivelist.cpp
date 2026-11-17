// DriveList: the probe's drives (S1), and actions from the engine's
// verdict alone (S12, ADR-0002; docs/specs/DFR-0002-engine-protocol.md
// § 4.5 names this test). The real engine comes from DEFRAGR_ENGINE;
// fakes, such as a crashing one, from DEFRAGR_FAKES.
#include "model/DriveList.h"

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QTest>
#include <QTextDocument>

using namespace defragr::engine;
using namespace defragr::model;

namespace {

Drive drive(quint8 verdict, quint16 reason, quint8 media)
{
    Drive made;
    made.verdict = verdict;
    made.reason = reason;
    made.media = media;
    made.totalBlocks = 1000;
    made.blockSize = 4096;
    made.mountPoint = "/mnt/Games";
    made.source = "/dev/sdb1";
    made.fsType = "ext4";
    return made;
}

struct Load {
    int loaded = 0;
    QList<Message> failures;
};

Load refresh(DriveList &list)
{
    Load load;
    QObject::connect(&list, &DriveList::loaded, [&] { ++load.loaded; });
    QObject::connect(&list, &DriveList::failed,
                     [&](const Message &message) { load.failures.append(message); });
    list.refresh();
    if (!QTest::qWaitFor([&] { return load.loaded + load.failures.size() > 0; }, 10000))
        qWarning("the probe did not end");
    return load;
}

}  // namespace

class TestDriveList : public QObject {
    Q_OBJECT

    QByteArray m_engine;
    QDir m_fakes;

private slots:
    void initTestCase()
    {
        m_engine = qgetenv("DEFRAGR_ENGINE");
        QVERIFY2(!m_engine.isEmpty(), "DEFRAGR_ENGINE is not set");
        m_fakes.setPath(qEnvironmentVariable("DEFRAGR_FAKES"));
        QVERIFY2(m_fakes.exists(QStringLiteral("crash.sh")), "DEFRAGR_FAKES is not set");
    }

    void init() { qputenv("DEFRAGR_ENGINE", m_engine); }

    void actionsFollowTheVerdict_data()
    {
        QTest::addColumn<quint8>("verdict");
        QTest::addColumn<quint16>("reason");
        QTest::addColumn<quint8>("media");
        QTest::addColumn<bool>("report");
        QTest::addColumn<bool>("defrag");

        QTest::newRow("spinning ext4") << quint8(VerdictReportAndDefrag) << quint16(ReasonNone)
                                       << quint8(MediaSpinning) << true << true;
        // S12's own example: an SSD keeps the report and loses defrag.
        QTest::newRow("SSD") << quint8(VerdictReportOnly) << quint16(ReasonSsd)
                             << quint8(MediaSsd) << true << false;
        QTest::newRow("btrfs") << quint8(VerdictReportOnly) << quint16(ReasonFsNotExt4)
                               << quint8(MediaSpinning) << true << false;
        // The window never re-derives: a verdict of 2 is obeyed even where
        // the media field would have said no.
        QTest::newRow("verdict over media") << quint8(VerdictReportAndDefrag)
                                            << quint16(ReasonNone) << quint8(MediaSsd) << true
                                            << true;
        // A verdict this version does not know allows nothing.
        QTest::newRow("unknown verdict") << quint8(3) << quint16(0) << quint8(MediaSpinning)
                                         << false << false;
    }

    void actionsFollowTheVerdict()
    {
        QFETCH(quint8, verdict);
        QFETCH(quint16, reason);
        QFETCH(quint8, media);
        QFETCH(bool, report);
        QFETCH(bool, defrag);

        DriveList list;
        list.setDrives({drive(verdict, reason, media)});
        QCOMPARE(list.canReport(0), report);
        QCOMPARE(list.canDefrag(0), defrag);
    }

    // S1: each drive shows its size, filesystem and whether it is an SSD.
    void columns()
    {
        DriveList list;
        list.setDrives({drive(VerdictReportAndDefrag, ReasonNone, MediaSpinning)});
        const auto cell = [&](int column) { return list.index(0, column).data().toString(); };
        QCOMPARE(cell(DriveList::ColumnDrive), QStringLiteral("/mnt/Games"));
        QCOMPARE(cell(DriveList::ColumnSize), QLocale().formattedDataSize(4096000));
        QCOMPARE(cell(DriveList::ColumnFileSystem), QStringLiteral("ext4"));
        QCOMPARE(cell(DriveList::ColumnMedia), messages::media(MediaSpinning));
        QCOMPARE(cell(DriveList::ColumnActions), messages::verdict(VerdictReportAndDefrag));
    }

    // DFR-0047: a filesystem name is anybody's to choose, as a FUSE mount
    // shows, so the tooltip shows it as text, never as markup. A tooltip
    // is markup when Qt::mightBeRichText says so.
    void tooltipShowsNamesAsText()
    {
        Drive fuse = drive(VerdictReportOnly, ReasonFsNotExt4, MediaSpinning);
        fuse.fsType = "fuse.<img src=x>";
        DriveList list;
        list.setDrives({fuse});
        const QString tip = list.index(0, DriveList::ColumnDrive).data(Qt::ToolTipRole).toString();
        QString shown = tip;
        if (Qt::mightBeRichText(tip)) {
            QTextDocument markup;
            markup.setHtml(tip);
            shown = markup.toPlainText();
        }
        QCOMPARE(shown, messages::whyNoDefrag(fuse));
    }

    // S1: opening the list runs the real probe and shows its drives.
    void loadsFromTheEngine()
    {
        DriveList list;
        const Load load = refresh(list);
        QCOMPARE(load.failures.size(), 0);
        QCOMPARE(load.loaded, 1);
        for (int row = 0; row < list.rowCount(); ++row)
            QVERIFY(list.drive(row).mountPoint.startsWith('/'));
        qInfo("the list shows %d drives", list.rowCount());
    }

    // A broken probe shows a message and no drives, never a guess.
    void brokenEngineEmptiesTheList()
    {
        DriveList list;
        list.setDrives({drive(VerdictReportAndDefrag, ReasonNone, MediaSpinning)});
        qputenv("DEFRAGR_ENGINE", QFile::encodeName(m_fakes.filePath(QStringLiteral("crash.sh"))));
        const Load load = refresh(list);
        QCOMPARE(load.loaded, 0);
        QCOMPARE(load.failures.size(), 1);
        QVERIFY(!load.failures.first().what.isEmpty());
        QCOMPARE(list.rowCount(), 0);
    }

    // DFR-0049: the window can refresh the list. A refresh while a probe
    // runs is ignored, and keeps the drives that probe has already sent.
    // The fake sends its drive at once, then pauses for a second.
    void refreshWhileListingIsIgnored()
    {
        qputenv("DEFRAGR_ENGINE",
                QFile::encodeName(m_fakes.filePath(QStringLiteral("slow-probe.sh"))));
        DriveList list;
        int loaded = 0;
        int failed = 0;
        connect(&list, &DriveList::loaded, this, [&] { ++loaded; });
        connect(&list, &DriveList::failed, this, [&] { ++failed; });
        list.refresh();
        QVERIFY(list.isListing());
        QTest::qWait(500);
        list.refresh();
        QVERIFY(QTest::qWaitFor([&] { return loaded + failed > 0; }, 10000));
        QCOMPARE(failed, 0);
        QCOMPARE(loaded, 1);
        QCOMPARE(list.rowCount(), 1);
        QCOMPARE(list.drive(0).mountPoint, QByteArray("/mnt/slow"));
        QVERIFY(!list.isListing());
    }

    // ADR-0002: a missing engine is said plainly.
    void missingEngineSaysSo()
    {
        const QString path = QStringLiteral("/nonexistent/defragr-engine");
        qputenv("DEFRAGR_ENGINE", QFile::encodeName(path));
        DriveList list;
        const Load load = refresh(list);
        QCOMPARE(load.failures.size(), 1);
        QVERIFY(load.failures.first().what.contains(path));
    }
};

QTEST_GUILESS_MAIN(TestDriveList)
#include "tst_drivelist.moc"
