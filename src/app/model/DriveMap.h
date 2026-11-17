// DriveMap: where a drive's blocks lie, from the engine's MAP_RUNS
// records (docs/specs/DFR-0002-engine-protocol.md § 4.6), sorted into
// as many map cells as the window has room for (S4).
#pragma once

#include "engine/Records.h"

#include <QList>

namespace defragr::model {

// What one map cell shows.
enum class CellState { Free, Filesystem, Whole, Fragmented };

class DriveMap {
public:
    void clear() { m_records.clear(); }
    void add(const engine::MapRuns &runs) { m_records.append(runs); }
    bool isEmpty() const { return blocks() == 0; }

    // One past the last block any run names. A drive whose GETFSMAP
    // worked has free or filesystem runs to its end; one whose GETFSMAP
    // failed ends the map at its last file block.
    quint64 blocks() const;

    // The map in at most `count` cells, first block first. Each cell holds
    // the same number of blocks, the last perhaps fewer, so there can be
    // fewer cells than asked for. A cell shows the kind of block filling
    // most of it; a tie goes to the kind listed later in CellState. Blocks
    // in no run (§ 4.6: folders, extent trees, skipped files) count as the
    // filesystem's. Runs of a kind this window does not know are ignored.
    QList<CellState> cells(int count) const;

private:
    QList<engine::MapRuns> m_records;
};

}  // namespace defragr::model
