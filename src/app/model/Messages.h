// Messages: every engine code and refusal reason in plain words, with
// what the user can do, and how a path is shown. The only home of
// user-facing text about an engine result (docs/design.md § What may
// depend on what); the window never shows a raw code. The codes are
// docs/specs/DFR-0002-engine-protocol.md § 4.5 to § 4.7 and § 4.12.
#pragma once

#include "engine/EngineProcess.h"
#include "engine/Records.h"

#include <QString>

namespace defragr::model {

struct Message {
    QString what;    // what happened
    QString action;  // what the user can do about it; may be empty
    bool operator==(const Message &) const = default;
};

namespace messages {

// A path as the window shows it, marked to read left to right. In a
// right-to-left window an unmarked "/mnt/Games" shows as "mnt/Games/".
// A character in it that steers text direction shows as its code, such
// as [U+202E], so a name cannot reorder the sentence around it.
QString path(const QString &path);
QString path(const QByteArray &path);  // decoded with QFile::decodeName

// The number to give tr() beside a sentence with a count, so Qt picks the
// right plural form. Qt takes an int and a count can be bigger; its
// plural rules read a number's last two digits and whether it is small,
// so a huge count keeps both. The sentence shows the real count as %1.
int pluralOf(quint64 count);

// A filesystem's name, shown as a path is: FUSE lets anyone choose one.
QString fsType(const QByteArray &fsType);

// DRIVE.media, as a column entry: "SSD", "Spinning drive" or "Unknown".
QString media(quint8 media);

// DRIVE.verdict, as a column entry.
QString verdict(quint8 verdict);

// Why the drive cannot be defragged. Empty when its verdict allows it.
QString whyNoDefrag(const engine::Drive &drive);

// What the user can do with a drive whose verdict allows defrag.
QString defragAllowed();

// The probe finished and found no drives.
QString noDrives();

// The question before a defrag of `folder`, a path relative to the
// mount point ("" for the whole drive): `what` asks it, `action` says
// what the run does and cannot do.
Message confirmDefrag(const QByteArray &mountPoint, const QByteArray &folder);

// The folder picked for a defrag is not on the drive picked.
Message folderNotOnDrive(const QByteArray &mountPoint);

// Why a defrag or a report skipped a file or folder (FILE_SKIPPED,
// § 4.6 and § 4.12), as one sentence.
QString whySkipped(const engine::FileSkipped &skipped);

// The label of the button that fetches the drive list again, which
// advice about the list names.
QString refreshButton();

// How a run ended, when it did not end well. Empty for FINISHED OK.
Message forRun(const engine::RunOutcome &outcome);

}  // namespace messages

}  // namespace defragr::model
