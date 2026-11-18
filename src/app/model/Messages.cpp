#include "model/Messages.h"

#include <QCoreApplication>
#include <QFile>

#include <cstring>

namespace defragr::model::messages {

using namespace engine;

namespace {

// For the struct's tr(): lupdate files these sentences under "Messages".
struct Tr {
    Q_DECLARE_TR_FUNCTIONS(Messages)
};

// The failing call's error in words, e.g. "Permission denied". An errno
// the system has no name for would read "Unknown error 9999", and 0
// "Success", so those get plain words instead.
QString errorText(qint32 errnum)
{
    if (errnum <= 0 || !strerrorname_np(errnum))
        //: Stands in for the system's words for an error, when it has none.
        return Tr::tr("no details given");
    return QString::fromLocal8Bit(std::strerror(errnum));
}

// A function, not a constant: a constant is built before any translation
// is loaded, so it would stay English.
QString reportBug()
{
    return Tr::tr("This is a fault in DeFragR. Please report it on the project's GitHub page.");
}

Message forResult(const Finished &finished)
{
    switch (finished.result) {
    case ResultOk:
        return {};
    case ResultNotRoot:
        return {Tr::tr("The engine was not given admin rights."),
                Tr::tr("Try again and enter your password when asked.")};
    case ResultNotAMount:
        return {Tr::tr("That folder is not the top of a drive."),
                //: %1 is the label of the Refresh drives button.
                Tr::tr("Go back, press %1, and pick the drive again.")
                    .arg(refreshButton())};
    case ResultMountinfo:
        //: %1 is the system's own words for the error, such as "Permission denied".
        return {Tr::tr("DeFragR could not read the list of mounted drives (%1).")
                    .arg(errorText(finished.errnum)),
                Tr::tr("Try again. If it keeps happening, restart the PC.")};
    case ResultDriveError:
        //: %1 is the system's own words for the error, such as "Input/output error".
        return {Tr::tr("The drive gave an error: %1.").arg(errorText(finished.errnum)),
                Tr::tr("Check the drive is still connected, then try again.")};
    case ResultRefused:
        return {Tr::tr("The engine refused, because this drive no longer allows it."),
                //: %1 is the label of the Refresh drives button.
                Tr::tr("The drive may have changed. Go back and press %1 to see "
                       "what DeFragR can do with it now.")
                    .arg(refreshButton())};
    case ResultCancelled:
        return {Tr::tr("The run was cancelled."), {}};
    case ResultUsage:
    case ResultBadCommand:
    case ResultInternal:
        return {Tr::tr("The engine stopped with an internal problem."), reportBug()};
    }
    return {Tr::tr("The engine stopped for a reason this version of DeFragR does not know."),
            Tr::tr("A newer version of DeFragR may explain it.")};
}

}  // namespace

QString path(const QString &path)
{
    // A name holding its own POP DIRECTIONAL ISOLATE could end the isolate
    // early, and a RIGHT-TO-LEFT OVERRIDE after it reorder the sentence.
    // So every character that steers text direction shows as its code.
    QString shown;
    shown.reserve(path.size() + 2);
    // LEFT-TO-RIGHT ISOLATE and POP DIRECTIONAL ISOLATE: the path reads
    // left to right inside any sentence, and leaves the sentence alone.
    shown += QChar(0x2066);
    for (const QChar c : path) {
        const char16_t u = c.unicode();
        const bool steers = u == 0x061C || u == 0x200E || u == 0x200F
                            || (u >= 0x202A && u <= 0x202E) || (u >= 0x2066 && u <= 0x2069);
        if (steers)
            shown += QStringLiteral("[U+%1]").arg(uint(u), 4, 16, QLatin1Char('0')).toUpper();
        else
            shown += c;
    }
    shown += QChar(0x2069);
    return shown;
}

QString path(const QByteArray &path)
{
    return messages::path(QFile::decodeName(path));
}

QString fsType(const QByteArray &fsType)
{
    return path(fsType);
}

QString media(quint8 media)
{
    switch (media) {
    case MediaSsd:
        return Tr::tr("SSD");
    case MediaSpinning:
        return Tr::tr("Spinning drive");
    }
    return Tr::tr("Unknown");
}

QString verdict(quint8 verdict)
{
    switch (verdict) {
    case VerdictReportAndDefrag:
        return Tr::tr("Report and defrag");
    case VerdictReportOnly:
        return Tr::tr("Report only");
    }
    return Tr::tr("Nothing");
}

QString whyNoDefrag(const Drive &drive)
{
    if (drive.verdict == VerdictReportAndDefrag)
        return {};
    if (drive.verdict != VerdictReportOnly)
        return Tr::tr("This version of DeFragR does not understand what the engine said "
                      "about this drive, so it offers nothing for it. A newer version may.");
    switch (drive.reason) {
    case ReasonFsNotExt4:
        //: %1 is a filesystem's name, such as btrfs. Keep "ext4" as it is.
        return Tr::tr("This drive uses %1. DeFragR defrags ext4 drives only, "
                      "but it can still report on this one.")
            .arg(fsType(drive.fsType));
    case ReasonReadOnly:
        return Tr::tr("This drive is read-only, so nothing on it can be moved. "
                      "DeFragR can still report on it.");
    case ReasonSsd:
        return Tr::tr("This is a solid-state drive (SSD). Defragging wears an SSD out "
                      "and does not make it faster. DeFragR can still report on it.");
    case ReasonMediaUnknown:
        return Tr::tr("DeFragR cannot tell whether this is a solid-state drive, so it "
                      "will not defrag it, to be safe. It can still report on it.");
    }
    return Tr::tr("The engine will not defrag this drive, for a reason this version of "
                  "DeFragR does not know. It can still report on it.");
}

QString defragAllowed()
{
    return Tr::tr("DeFragR can report on this drive or defrag it.");
}

QString noDrives()
{
    return Tr::tr("DeFragR found no drives to list.");
}

QString defragComesLater()
{
    return Tr::tr("Defragging comes in a later version of DeFragR. The engine has "
                  "checked this drive and will allow it.");
}

QString refreshButton()
{
    return Tr::tr("Refresh drives");
}

Message forRun(const RunOutcome &outcome)
{
    switch (outcome.end) {
    case RunEnd::Finished:
        return forResult(outcome.finished);
    case RunEnd::EngineMissing:
        //: %1 is the path where the engine should be.
        return {Tr::tr("DeFragR's engine is missing. It should be at %1.")
                    .arg(path(outcome.enginePath)),
                Tr::tr("Reinstall DeFragR. Without its engine it cannot list or check "
                       "drives.")};
    case RunEnd::LauncherMissing:
        return {Tr::tr("DeFragR could not ask for admin rights, because pkexec, the "
                       "program that asks for them, did not start."),
                Tr::tr("Check that pkexec is installed. It comes with polkit. Then try "
                       "again.")};
    case RunEnd::AdminRefused:
        // pkexec's 127 also covers the system blocking the engine after
        // the password, so the words blame neither (DFR-0042).
        return {Tr::tr("DeFragR could not start its engine with admin rights."),
                Tr::tr("Try again and enter your password when asked. If it still "
                       "fails after you enter it, a security setting on this PC, such "
                       "as SELinux, may be blocking the engine.")};
    case RunEnd::VersionMismatch:
        return {Tr::tr("DeFragR's engine belongs to a different version of DeFragR."),
                Tr::tr("Reinstall DeFragR so both halves match.")};
    case RunEnd::Broken:
        break;
    }
    return {Tr::tr("The engine stopped unexpectedly."), reportBug()};
}

}  // namespace defragr::model::messages
