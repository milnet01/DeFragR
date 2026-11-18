// DriveMap: where a drive's blocks lie, from the engine's MAP_RUNS
// records (docs/specs/DFR-0002-engine-protocol.md § 4.6), sorted into
// as many map cells as the window has room for (S4).
//
// A local user who can write to the drive chooses how many runs there
// are, so the runs are not kept. Each is tallied into bins: blocks of each
// kind per bin, at most kBinsMax bins, 8 MiB. A bin starts one block
// wide; when the map outgrows kBinsMax bins, neighbours merge and bins
// double in width. A map of up to kBinsMax blocks is exact.
#pragma once

#include "engine/Records.h"

#include <QList>

#include <array>
#include <cstddef>

namespace defragr::model {

// What one map cell shows.
enum class CellState { Free, Filesystem, Whole, Fragmented };
// How many states there are, for arrays indexed by CellState. A state
// added after Fragmented fails the assert until this counts it.
inline constexpr std::size_t kCellStates = 4;
static_assert(kCellStates == std::size_t(CellState::Fragmented) + 1);

class DriveMap {
public:
    static constexpr qsizetype kBinsMax = qsizetype(1) << 18;

    void clear();
    void add(const engine::MapRuns &runs);
    bool isEmpty() const { return m_end == 0; }

    // One past the last block any run names. A drive whose GETFSMAP
    // worked has free or filesystem runs to its end; one whose GETFSMAP
    // failed ends the map at its last file block.
    quint64 blocks() const { return m_end; }

    // The map in at most `count` cells, first block first. Each cell holds
    // the same number of blocks, the last perhaps fewer, so there can be
    // fewer cells than asked for. A cell shows the kind of block filling
    // most of it; a tie goes to the kind listed later in CellState. Blocks
    // in no run (§ 4.6: folders, extent trees, skipped files) count as the
    // filesystem's. Runs of a kind this window does not know are ignored.
    // Where bins are wider than a block, a bin across two cells shares
    // its blocks between them by width.
    QList<CellState> cells(int count) const;

private:
    // Blocks of each kind, indexed by CellState.
    using Tally = std::array<quint64, kCellStates>;

    // Widens the bins until m_end fits in kBinsMax of them, then adds
    // bins up to m_end.
    void fit();

    QList<Tally> m_bins;
    quint64 m_binBlocks = 1;  // each bin's width, a power of two
    quint64 m_end = 0;
};

}  // namespace defragr::model
