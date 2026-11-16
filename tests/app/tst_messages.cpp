// Messages has words for every engine code, and never shows a raw one
// (docs/design.md § Errors; the codes are
// docs/specs/DFR-0002-engine-protocol.md § 4.5 and § 4.7).
#include "model/Messages.h"

#include <QRegularExpression>
#include <QSet>
#include <QTest>

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
        for (RunEnd end : {RunEnd::EngineMissing, RunEnd::AdminRefused, RunEnd::Broken,
                           RunEnd::VersionMismatch}) {
            RunOutcome outcome;
            outcome.end = end;
            outcome.enginePath = QStringLiteral("/opt/defragr/defragr-engine");
            const Message message = messages::forRun(outcome);
            QVERIFY(!message.what.isEmpty());
            QVERIFY(!message.action.isEmpty());
        }
    }

    // ADR-0002: the user is told where the missing engine should be.
    void missingEngineIsNamed()
    {
        RunOutcome outcome;
        outcome.end = RunEnd::EngineMissing;
        outcome.enginePath = QStringLiteral("/opt/defragr/defragr-engine");
        QVERIFY(messages::forRun(outcome).what.contains(outcome.enginePath));
    }

    // A failing call's errno is shown as words, never as its number.
    void errnoIsWords()
    {
        RunOutcome outcome = finishedWith(ResultDriveError);
        outcome.finished.errnum = 13;  // EACCES
        const Message message = messages::forRun(outcome);
        QVERIFY2(!hasDigit(message.what), qPrintable(message.what));
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
