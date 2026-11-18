// Messages: every engine code and refusal reason in plain words, with
// what the user can do, and how a path is shown. The only home of
// user-facing text about an engine result (docs/design.md § What may
// depend on what); the window never shows a raw code. The codes are
// docs/specs/DFR-0002-engine-protocol.md § 4.5 and § 4.7.
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

// What "Defrag now" says until defragging is built (0.3.0).
QString defragComesLater();

// The label of the button that fetches the drive list again, which
// advice about the list names.
QString refreshButton();

// How a run ended, when it did not end well. Empty for FINISHED OK.
Message forRun(const engine::RunOutcome &outcome);

}  // namespace messages

}  // namespace defragr::model
