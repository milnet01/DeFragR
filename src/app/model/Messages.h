// Messages: every engine code and refusal reason in plain words, with
// what the user can do. The only home of user-facing text about an
// engine result (docs/design.md § What may depend on what); the window
// never shows a raw code. The codes are
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
