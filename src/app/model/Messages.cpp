#include "model/Messages.h"

#include <QFile>

#include <cstring>

namespace defragr::model::messages {

using namespace engine;

namespace {

// The failing call's error in words, e.g. "Permission denied".
QString errorText(qint32 errnum)
{
    return QString::fromLocal8Bit(std::strerror(errnum));
}

const QString kReportBug = QStringLiteral(
    "This is a fault in DeFragR. Please report it on the project's GitHub page.");

Message forResult(const Finished &finished)
{
    switch (finished.result) {
    case ResultOk:
        return {};
    case ResultNotRoot:
        return {QStringLiteral("The engine was not given admin rights."),
                QStringLiteral("Try again and enter your password when asked.")};
    case ResultNotAMount:
        return {QStringLiteral("That folder is not the top of a drive."),
                QStringLiteral("Pick the drive from the list again.")};
    case ResultMountinfo:
        return {QStringLiteral("DeFragR could not read the list of mounted drives (%1).")
                    .arg(errorText(finished.errnum)),
                QStringLiteral("Try again. If it keeps happening, restart the PC.")};
    case ResultDriveError:
        return {QStringLiteral("The drive gave an error: %1.").arg(errorText(finished.errnum)),
                QStringLiteral("Check the drive is still connected, then try again.")};
    case ResultRefused:
        return {QStringLiteral("The engine refused, because this drive no longer allows it."),
                QStringLiteral("The drive may have changed. Check the list again.")};
    case ResultCancelled:
        return {QStringLiteral("The run was cancelled."), {}};
    case ResultUsage:
    case ResultBadCommand:
    case ResultInternal:
        return {QStringLiteral("The engine stopped with an internal problem."), kReportBug};
    }
    return {QStringLiteral("The engine stopped for a reason this version of DeFragR does not know."),
            QStringLiteral("A newer version of DeFragR may explain it.")};
}

}  // namespace

QString media(quint8 media)
{
    switch (media) {
    case MediaSsd:
        return QStringLiteral("SSD");
    case MediaSpinning:
        return QStringLiteral("Spinning drive");
    }
    return QStringLiteral("Unknown");
}

QString verdict(quint8 verdict)
{
    switch (verdict) {
    case VerdictReportAndDefrag:
        return QStringLiteral("Report and defrag");
    case VerdictReportOnly:
        return QStringLiteral("Report only");
    }
    return QStringLiteral("Nothing");
}

QString whyNoDefrag(const Drive &drive)
{
    if (drive.verdict == VerdictReportAndDefrag)
        return {};
    if (drive.verdict != VerdictReportOnly)
        return QStringLiteral("This version of DeFragR does not understand what the engine said "
                              "about this drive, so it offers nothing for it. A newer version may.");
    switch (drive.reason) {
    case ReasonFsNotExt4:
        return QStringLiteral("This drive uses %1. DeFragR defrags ext4 drives only, "
                              "but it can still report on this one.")
            .arg(QFile::decodeName(drive.fsType));
    case ReasonReadOnly:
        return QStringLiteral("This drive is read-only, so nothing on it can be moved. "
                              "DeFragR can still report on it.");
    case ReasonSsd:
        return QStringLiteral("This is a solid-state drive (SSD). Defragging wears an SSD out "
                              "and does not make it faster. DeFragR can still report on it.");
    case ReasonMediaUnknown:
        return QStringLiteral("DeFragR cannot tell whether this is a solid-state drive, so it "
                              "will not defrag it, to be safe. It can still report on it.");
    }
    return QStringLiteral("The engine will not defrag this drive, for a reason this version of "
                          "DeFragR does not know. It can still report on it.");
}

Message forRun(const RunOutcome &outcome)
{
    switch (outcome.end) {
    case RunEnd::Finished:
        return forResult(outcome.finished);
    case RunEnd::EngineMissing:
        return {QStringLiteral("DeFragR's engine is missing. It should be at %1.")
                    .arg(outcome.enginePath),
                QStringLiteral("Reinstall DeFragR. Without its engine it cannot list or check "
                               "drives.")};
    case RunEnd::AdminRefused:
        // pkexec's 127 also covers the system blocking the engine after
        // the password, so the words blame neither (DFR-0042).
        return {QStringLiteral("DeFragR could not start its engine with admin rights."),
                QStringLiteral("Try again and enter your password when asked. If it still "
                               "fails after you enter it, a security setting on this PC, such "
                               "as SELinux, may be blocking the engine.")};
    case RunEnd::VersionMismatch:
        return {QStringLiteral("DeFragR's engine belongs to a different version of DeFragR."),
                QStringLiteral("Reinstall DeFragR so both halves match.")};
    case RunEnd::Broken:
        break;
    }
    return {QStringLiteral("The engine stopped unexpectedly."), kReportBug};
}

}  // namespace defragr::model::messages
