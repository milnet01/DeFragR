// RunLog: the developers' log (ROADMAP DFR-0111). A build made with
// DEFRAGR_LOG writes what the window and the engine did to a file, so a
// run that looked wrong can be read afterwards. It names the user's
// files and folders, so it stays on the machine; a released build
// starts none.
#pragma once

#include "engine/Records.h"

#include <QLoggingCategory>
#include <QString>

namespace defragr::engine {

// "defragr.run". Warnings always; the rest once the log has started.
Q_DECLARE_LOGGING_CATEGORY(runLog)

// The most the log file may hold. A file past it becomes the one older
// file kept, and a session stops writing when it gets there.
inline constexpr qint64 kRunLogBytes = qint64(8) << 20;

// $XDG_STATE_HOME/defragr, or ~/.local/state/defragr.
QString runLogFolder();

// Starts writing every Qt message, this category's among them, to
// defragr.log in folder, making the folder. False, and nothing changes,
// where the file cannot be opened.
bool startRunLog(const QString &folder);
// Stops, and closes the file. For tests: the window logs until it exits.
void stopRunLog();

// Which of a run's records the log notes. A run can send millions, so
// progress is noted when its whole percentage changes, and what only
// draws the map (MAP_RUNS, BLOCKS_MOVED, a file's own record) never.
class RecordNotes {
public:
    // The record as a line, or nothing where it is not noted.
    QString note(const Record &record);

private:
    bool percentChanged(quint64 done, quint64 total, int *last);

    int m_reportPercent = -1;
    int m_defragPercent = -1;
};

}  // namespace defragr::engine
