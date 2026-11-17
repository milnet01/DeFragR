#include "model/DriveMap.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

using namespace defragr::engine;

namespace defragr::model {

namespace {

// One past a run's last block. The decoder takes any start and length
// (INV-16), so the sum saturates rather than wrap.
quint64 endOf(const MapRun &run)
{
    return run.start > std::numeric_limits<quint64>::max() - run.length
               ? std::numeric_limits<quint64>::max()
               : run.start + run.length;
}

std::optional<CellState> stateOf(quint32 kind)
{
    switch (kind) {
    case MapFree:
        return CellState::Free;
    case MapFilesystem:
        return CellState::Filesystem;
    case MapWholeFile:
        return CellState::Whole;
    case MapFragmentedFile:
        return CellState::Fragmented;
    }
    return std::nullopt;
}

}  // namespace

quint64 DriveMap::blocks() const
{
    quint64 end = 0;
    for (const MapRuns &record : m_records)
        if (stateOf(record.kind))
            for (const MapRun &run : record.runs)
                end = std::max(end, endOf(run));
    return end;
}

QList<CellState> DriveMap::cells(int count) const
{
    const quint64 total = blocks();
    if (count <= 0 || total == 0)
        return {};
    const quint64 perCell = total / quint64(count) + (total % quint64(count) != 0);
    const qsizetype cellCount = qsizetype((total - 1) / perCell + 1);

    // Blocks of each kind in each cell, indexed by CellState.
    QList<std::array<quint64, 4>> tally(cellCount, std::array<quint64, 4>{});
    for (const MapRuns &record : m_records) {
        const std::optional<CellState> state = stateOf(record.kind);
        if (!state)
            continue;
        for (const MapRun &run : record.runs) {
            const quint64 end = endOf(run);
            for (quint64 at = run.start; at < end;) {
                const quint64 cell = at / perCell;
                // The last cell's end is the run's, so near the top of
                // the range nothing overflows.
                const quint64 cellEnd = cell + 1 == quint64(cellCount)
                                            ? end
                                            : std::min(end, (cell + 1) * perCell);
                tally[qsizetype(cell)][size_t(*state)] += cellEnd - at;
                at = cellEnd;
            }
        }
    }

    QList<CellState> cells;
    cells.reserve(cellCount);
    for (qsizetype i = 0; i < cellCount; ++i) {
        std::array<quint64, 4> &blocks = tally[i];
        const quint64 size = std::min(perCell, total - quint64(i) * perCell);
        quint64 named = 0;
        for (const quint64 n : blocks)
            named += n;
        // Overlapping runs (§ 4.6) can name more blocks than the cell has.
        if (named < size)
            blocks[size_t(CellState::Filesystem)] += size - named;
        // max_element keeps the first of equals; searching from the end
        // gives a tie to the later kind.
        const auto most = std::max_element(blocks.rbegin(), blocks.rend());
        cells.append(CellState(blocks.rend() - most - 1));
    }
    return cells;
}

}  // namespace defragr::model
