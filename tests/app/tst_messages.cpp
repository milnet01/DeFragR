// Messages has words for every engine code, and never shows a raw one
// (docs/design.md § Errors; the codes are
// docs/specs/DFR-0002-engine-protocol.md § 4.5 and § 4.7).
#include "model/Messages.h"

#include <QRegularExpression>
#include <QSet>
#include <QTest>

#include <cstring>

using namespace defragr::engine;
using namespace defragr::model;

namespace {

RunOutcome finishedWith(qint32 result)
{
    RunOutcome outcome;
    outcome.end = RunEnd::Finished;
    outcome.finished.result = result;
    return outcome;
}

Drive reportOnly(quint16 reason)
{
    Drive drive;
    drive.verdict = VerdictReportOnly;
    drive.reason = reason;
    drive.fsType = "btrfs";
    return drive;
}

bool hasDigit(const QString &text)
{
    return text.contains(QRegularExpression(QStringLiteral("[0-9]")));
}

}  // namespace

class TestMessages : public QObject {
    Q_OBJECT

private slots:
    void finishedOkSaysNothing()
    {
        QCOMPARE(messages::forRun(finishedWith(ResultOk)), Message{});
    }

    void everyResultHasWords()
    {
        for (qint32 result = ResultUsage; result <= ResultInternal; ++result) {
            const Message message = messages::forRun(finishedWith(result));
            QVERIFY2(!message.what.isEmpty(), qPrintable(QString::number(result)));
        }
    }

    // Every way a run can end badly says what happened and what to do.
    void everyRunEndHasWords()
    {
        for (RunEnd end : {RunEnd::EngineMissing, RunEnd::LauncherMissing, RunEnd::AdminRefused,
                           RunEnd::Broken, RunEnd::VersionMismatch}) {
            RunOutcome outcome;
            outcome.end = end;
            outcome.enginePath = QStringLiteral("/opt/defragr/defragr-engine");
            const Message message = messages::forRun(outcome);
            QVERIFY(!message.what.isEmpty());
            QVERIFY(!message.action.isEmpty());
        }
    }

    // pkexec exits 127 both when the user is refused and when the system
    // stops it running the engine after the password (SELinux, seen
    // 2026-11-16), so the words cover both and blame neither (DFR-0042).
    void adminRightsCoverABlockedEngine()
    {
        RunOutcome outcome;
        outcome.end = RunEnd::AdminRefused;
        QCOMPARE(messages::forRun(outcome),
                 (Message{QStringLiteral("DeFragR could not start its engine with admin rights."),
                          QStringLiteral("Try again and enter your password when asked. If it "
                                         "still fails after you enter it, a security setting "
                                         "on this PC, such as SELinux, may be blocking the "
                                         "engine.")}));
    }

    // DFR-0049: when pkexec cannot start, no password prompt can appear,
    // so the words name pkexec and never ask for a password.
    void missingLauncherIsNamed()
    {
        RunOutcome outcome;
        outcome.end = RunEnd::LauncherMissing;
        const Message message = messages::forRun(outcome);
        QVERIFY2(message.what.contains(QStringLiteral("pkexec")), qPrintable(message.what));
        QVERIFY2(!(message.what + message.action).contains(QStringLiteral("password")),
                 qPrintable(message.action));
    }

    // DFR-0049: advice to look at the drive list again names the button
    // that refreshes it, since the list is otherwise never fetched again.
    void listAdviceNamesTheRefreshButton()
    {
        for (qint32 result : {ResultNotAMount, ResultRefused}) {
            const Message message = messages::forRun(finishedWith(result));
            QVERIFY2(message.action.contains(messages::refreshButton()),
                     qPrintable(message.action));
        }
    }

    // ADR-0002: the user is told where the missing engine should be.
    void missingEngineIsNamed()
    {
        RunOutcome outcome;
        outcome.end = RunEnd::EngineMissing;
        outcome.enginePath = QStringLiteral("/opt/defragr/defragr-engine");
        // DFR-0069: marked left to right, as every path is.
        QVERIFY(messages::forRun(outcome).what.contains(
            QStringLiteral(u"\u2066/opt/defragr/defragr-engine\u2069")));
    }

    // A failing call's errno is shown as words, never as its number.
    void errnoIsWords()
    {
        RunOutcome outcome = finishedWith(ResultDriveError);
        outcome.finished.errnum = 13;  // EACCES
        const Message message = messages::forRun(outcome);
        QVERIFY2(!hasDigit(message.what), qPrintable(message.what));
    }

    // DFR-0051: an errno the system has no words for is not shown as a
    // number, and 0 is not shown as "Success".
    void unnamedErrnoIsWords()
    {
        for (const qint32 errnum : {0, -5, 9999}) {
            RunOutcome outcome = finishedWith(ResultDriveError);
            outcome.finished.errnum = errnum;
            const QString what = messages::forRun(outcome).what;
            QVERIFY2(!hasDigit(what), qPrintable(what));
            QVERIFY2(!what.contains(QString::fromLocal8Bit(std::strerror(0))), qPrintable(what));
        }
    }

    // S9: each refusal has its own plain reason; a drive that may be
    // defragged has none.
    void everyReasonHasWords()
    {
        Drive allowed;
        allowed.verdict = VerdictReportAndDefrag;
        QCOMPARE(messages::whyNoDefrag(allowed), QString());

        QSet<QString> seen;
        for (quint16 reason = ReasonFsNotExt4; reason <= ReasonMediaUnknown; ++reason) {
            const QString why = messages::whyNoDefrag(reportOnly(reason));
            QVERIFY2(!why.isEmpty(), qPrintable(QString::number(reason)));
            QVERIFY2(!seen.contains(why), qPrintable(why));
            seen.insert(why);
        }
        QVERIFY(messages::whyNoDefrag(reportOnly(ReasonFsNotExt4)).contains(QStringLiteral("btrfs")));
    }

    // A code this version does not know still gets words, with no number
    // (INV-9 lets such codes through the decoder).
    void unknownCodesGetWordsNotNumbers()
    {
        const QString reason = messages::whyNoDefrag(reportOnly(200));
        QVERIFY(!reason.isEmpty());
        QVERIFY2(!hasDigit(reason), qPrintable(reason));

        Drive unknownVerdict;
        unknownVerdict.verdict = 3;
        const QString verdict = messages::whyNoDefrag(unknownVerdict);
        QVERIFY(!verdict.isEmpty());
        QVERIFY2(!hasDigit(verdict), qPrintable(verdict));
        QVERIFY(!hasDigit(messages::verdict(3)));
        QVERIFY(!hasDigit(messages::media(9)));

        const Message result = messages::forRun(finishedWith(99));
        QVERIFY(!result.what.isEmpty());
        QVERIFY2(!hasDigit(result.what + result.action), qPrintable(result.what));
    }

    void columnWords()
    {
        QCOMPARE(messages::media(MediaSsd), QStringLiteral("SSD"));
        QCOMPARE(messages::media(MediaSpinning), QStringLiteral("Spinning drive"));
        QCOMPARE(messages::media(MediaUnknown), QStringLiteral("Unknown"));
        QCOMPARE(messages::verdict(VerdictReportAndDefrag), QStringLiteral("Report and defrag"));
        QCOMPARE(messages::verdict(VerdictReportOnly), QStringLiteral("Report only"));
    }
};

QTEST_GUILESS_MAIN(TestMessages)
#include "tst_messages.moc"
