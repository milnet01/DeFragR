// DriveMap: where a drive's blocks lie, from the engine's MAP_RUNS
// records (docs/specs/DFR-0002-engine-protocol.md § 4.6), sorted into
// as many map cells as the window has room for (S4).
//
// A local user who can write to the drive chooses how many runs there
// are, so the runs are not kept. Each is tallied into bins: blocks of each
// kind per bin, at most kBinsMax bins, 8 MiB. A bin starts one block
// wide; when the map outgrows kBinsMax bins, neighbours merge and bins
// double in width. A map of up to kBinsMax blocks is exact.
//
// During a defrag the engine's BLOCKS_MOVED records (§ 4.12) change the
// tallies, so the map follows the drive without a new report
// (docs/specs/DFR-0018-window-folder-defrag.md § 4.1).
#pragma once

#include "engine/Records.h"

#include <QHash>
#include <QList>

#include <array>
#include <cstddef>

namespace defragr::model {

// What one map cell shows. LargePieces is a file in two or more pieces
// that average engine::LargePieceBytes or more; Fragmented, one whose
// pieces are smaller.
enum class CellState { Free, Filesystem, Whole, LargePieces, Fragmented };
// How many states there are, for arrays indexed by CellState. A state
// added after Fragmented fails the assert until this counts it.
inline constexpr std::size_t kCellStates = 5;
static_assert(kCellStates == std::size_t(CellState::Fragmented) + 1);

// A file's state from its pieces and the blocks they hold, as the engine
// grades a map's runs (DFR-0002 § 4.6): Whole, LargePieces or Fragmented.
CellState fileState(quint64 pieces, quint64 blocks, quint32 blockSize);

class DriveMap {
public:
    static constexpr qsizetype kBinsMax = qsizetype(1) << 18;

    void clear();
    void add(const engine::MapRuns &runs);
    bool isEmpty() const { return m_end == 0; }

    // A move the engine made: its from blocks now count as free, its to
    // blocks as a whole file's. A from block is a fragmented file's, or
    // else one in large pieces. The map may be stale or coarse, so where
    // a bin lacks the blocks a move names, they are taken from blocks in
    // no run, then from the other kinds. A run past the map's end grows
    // the map, as add() does.
    void move(const engine::BlocksMoved &move);
    // The file those moves belonged to is done, and is this state: where
    // it is not Whole, the blocks that landed for it count as that.
    void endTurn(CellState file);

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
    // How many blocks each of cells(count)'s cells holds, the last
    // perhaps fewer; 0 for an empty map or no cells.
    quint64 blocksPerCell(int count) const;

private:
    // Blocks of each kind, indexed by CellState.
    using Tally = std::array<quint64, kCellStates>;

    // Widens the bins until m_end fits in kBinsMax of them, then adds
    // bins up to m_end.
    void fit();
    // A bin's width in blocks: m_binBlocks, but the last bin ends at m_end.
    quint64 binWidth(qsizetype bin) const;
    // Counts the run's blocks as `now` where they counted as `was`.
    // With `landed`, notes how many reached each bin, for endTurn().
    void recount(const engine::MapRun &run, CellState was, CellState now, bool landed);

    // Blocks that landed in each bin during the file's turn under way.
    // At most one entry per bin, so it is bounded as m_bins is.
    QHash<qsizetype, quint64> m_landed;
    QList<Tally> m_bins;
    quint64 m_binBlocks = 1;  // each bin's width, a power of two
    quint64 m_end = 0;
};

}  // namespace defragr::model
