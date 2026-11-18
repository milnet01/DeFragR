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

void DriveMap::clear()
{
    m_bins.clear();
    m_binBlocks = 1;
    m_end = 0;
}

void DriveMap::fit()
{
    // Bins up to m_end, rounded up; m_end is at least 1 here.
    const auto needed = [&] { return (m_end - 1) / m_binBlocks + 1; };
    while (needed() > quint64(kBinsMax)) {
        for (qsizetype i = 0; i < m_bins.size(); i += 2) {
            Tally merged = m_bins[i];
            if (i + 1 < m_bins.size())
                for (size_t kind = 0; kind < merged.size(); ++kind)
                    merged[kind] += m_bins[i + 1][kind];
            m_bins[i / 2] = merged;
        }
        m_bins.resize((m_bins.size() + 1) / 2);
        m_binBlocks *= 2;
    }
    m_bins.resize(qsizetype(needed()), Tally{});
}

void DriveMap::add(const MapRuns &runs)
{
    const std::optional<CellState> state = stateOf(runs.kind);
    if (!state)
        return;
    for (const MapRun &run : runs.runs) {
        const quint64 end = endOf(run);
        if (end <= run.start)
            continue;
        if (end > m_end) {
            m_end = end;
            fit();
        }
        for (quint64 at = run.start; at < end;) {
            const quint64 bin = at / m_binBlocks;
            // The last bin's end is the map's, so near the top of the
            // range nothing overflows.
            const quint64 binEnd = bin + 1 == quint64(m_bins.size())
                                       ? end
                                       : std::min(end, (bin + 1) * m_binBlocks);
            m_bins[qsizetype(bin)][size_t(*state)] += binEnd - at;
            at = binEnd;
        }
    }
}

QList<CellState> DriveMap::cells(int count) const
{
    const quint64 total = m_end;
    if (count <= 0 || total == 0)
        return {};
    const quint64 perCell = total / quint64(count) + (total % quint64(count) != 0);
    const qsizetype cellCount = qsizetype((total - 1) / perCell + 1);

    // Blocks of each kind in each cell. Doubles, since a bin across two
    // cells is shared by width; a one-block bin stays a whole number.
    QList<std::array<double, 4>> tally(cellCount, std::array<double, 4>{});
    const qsizetype last = m_bins.size() - 1;
    for (qsizetype bin = 0; bin <= last; ++bin) {
        const quint64 start = quint64(bin) * m_binBlocks;
        const quint64 end = bin == last ? total : start + m_binBlocks;
        for (quint64 at = start; at < end;) {
            const quint64 cell = at / perCell;
            const quint64 cellEnd = cell + 1 == quint64(cellCount)
                                        ? end
                                        : std::min(end, (cell + 1) * perCell);
            const double share = double(cellEnd - at) / double(end - start);
            for (size_t kind = 0; kind < 4; ++kind)
                tally[qsizetype(cell)][kind] += double(m_bins[bin][kind]) * share;
            at = cellEnd;
        }
    }

    QList<CellState> cells;
    cells.reserve(cellCount);
    for (qsizetype i = 0; i < cellCount; ++i) {
        std::array<double, 4> &blocks = tally[i];
        const double size = double(std::min(perCell, total - quint64(i) * perCell));
        double named = 0;
        for (const double n : blocks)
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
