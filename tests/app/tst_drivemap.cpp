// DriveMap: the engine's map runs sorted into map cells (S4), each
// showing the kind of block filling most of it.
#include "model/DriveMap.h"

#include <QElapsedTimer>
#include <QTest>

#include <limits>

#include <malloc.h>

using namespace defragr::engine;
using defragr::model::CellState;
using defragr::model::DriveMap;

namespace {

MapRuns runs(quint32 kind, QList<MapRun> list)
{
    return MapRuns{kind, std::move(list)};
}

}  // namespace

class TestDriveMap : public QObject {
    Q_OBJECT

private slots:
    void emptyMapHasNoCells()
    {
        DriveMap map;
        QVERIFY(map.isEmpty());
        QCOMPARE(map.cells(10), QList<CellState>{});
        map.add(runs(MapFree, {{0, 8}}));
        QCOMPARE(map.cells(0), QList<CellState>{});
        map.clear();
        QVERIFY(map.isEmpty());
    }

    // One cell per block where there is room, in block order.
    void oneBlockPerCell()
    {
        DriveMap map;
        map.add(runs(MapFree, {{0, 1}}));
        map.add(runs(MapFilesystem, {{1, 1}}));
        map.add(runs(MapWholeFile, {{2, 1}}));
        map.add(runs(MapFragmentedFile, {{3, 1}}));
        QCOMPARE(map.blocks(), 4u);
        const QList<CellState> expected{CellState::Free, CellState::Filesystem,
                                        CellState::Whole, CellState::Fragmented};
        QCOMPARE(map.cells(4), expected);
        // More room than blocks still gives one cell per block.
        QCOMPARE(map.cells(100), expected);
    }

    // A cell shows what fills most of it, not what is most alarming: a
    // few fragmented blocks must not stripe a cell of whole files.
    void mostOfTheCellWins()
    {
        DriveMap map;
        map.add(runs(MapWholeFile, {{0, 7}}));
        map.add(runs(MapFragmentedFile, {{7, 3}, {10, 6}}));
        map.add(runs(MapFree, {{16, 2}}));
        // Cells of 9: 7 whole and 2 fragmented, then 7 fragmented and 2 free.
        QCOMPARE(map.cells(2), (QList<CellState>{CellState::Whole, CellState::Fragmented}));
    }

    void tieGoesToTheLaterKind()
    {
        DriveMap map;
        map.add(runs(MapFree, {{0, 2}}));
        map.add(runs(MapFragmentedFile, {{2, 2}}));
        map.add(runs(MapWholeFile, {{4, 2}}));
        map.add(runs(MapFilesystem, {{6, 2}}));
        QCOMPARE(map.cells(2), (QList<CellState>{CellState::Fragmented, CellState::Whole}));
    }

    // Blocks in no run are folders, extent trees and the like (§ 4.6):
    // in use, so never shown as free.
    void unnamedBlocksAreTheFilesystems()
    {
        DriveMap map;
        map.add(runs(MapFree, {{0, 3}, {7, 3}}));
        // Cell 0 is 3 free and 2 unnamed; cell 1 is 2 unnamed and 3 free.
        QCOMPARE(map.cells(2), (QList<CellState>{CellState::Free, CellState::Free}));
        map.add(runs(MapFree, {{10, 1}}));
        // Cells of 4, 4 and 3: 3 free and 1 unnamed; 1 free and 3 unnamed;
        // 3 free.
        QCOMPARE(map.cells(3), (QList<CellState>{CellState::Free, CellState::Filesystem,
                                                 CellState::Free}));
    }

    // The last cell can be short; it is judged on its own blocks.
    void shortLastCell()
    {
        DriveMap map;
        map.add(runs(MapWholeFile, {{0, 9}}));
        map.add(runs(MapFree, {{9, 1}}));
        // Cells of 3, 3, 3 and 1.
        QCOMPARE(map.cells(4), (QList<CellState>{CellState::Whole, CellState::Whole,
                                                 CellState::Whole, CellState::Free}));
        // Cells of 2: five of them, not the six asked for.
        QCOMPARE(map.cells(6).size(), 5);
    }

    // A run spanning cells counts in each.
    void longRunSpansCells()
    {
        DriveMap map;
        map.add(runs(MapFree, {{0, 30}, {70, 30}}));
        map.add(runs(MapFragmentedFile, {{30, 40}}));
        QCOMPARE(map.cells(10),
                 (QList<CellState>{CellState::Free, CellState::Free, CellState::Free,
                                   CellState::Fragmented, CellState::Fragmented,
                                   CellState::Fragmented, CellState::Fragmented,
                                   CellState::Free, CellState::Free, CellState::Free}));
    }

    // A file seen through two hard links sends its runs twice (§ 4.6).
    void overlappingRunsDoNotMakeBlocksUp()
    {
        DriveMap map;
        map.add(runs(MapFragmentedFile, {{0, 4}}));
        map.add(runs(MapFragmentedFile, {{0, 4}}));
        map.add(runs(MapFree, {{4, 4}}));
        QCOMPARE(map.cells(2), (QList<CellState>{CellState::Fragmented, CellState::Free}));
    }

    // A kind this window does not know is skipped, never guessed (INV-9).
    void unknownKindIgnored()
    {
        DriveMap map;
        map.add(runs(99, {{0, 1000}}));
        QVERIFY(map.isEmpty());
        map.add(runs(MapWholeFile, {{0, 2}}));
        QCOMPARE(map.blocks(), 2u);
    }

    // A run reaching the top of the block range neither wraps nor hangs.
    void runAtTheTopOfTheRange()
    {
        constexpr quint64 top = std::numeric_limits<quint64>::max();
        DriveMap map;
        map.add(runs(MapFree, {{0, top / 2}}));
        map.add(runs(MapWholeFile, {{top / 2, top}}));
        QCOMPARE(map.blocks(), top);
        QCOMPARE(map.cells(2), (QList<CellState>{CellState::Free, CellState::Whole}));
    }

    // A map of a big drive bins quickly enough to redo on every resize.
    // The limit is dozens of times an unoptimised build's time on a busy
    // machine, so it catches binning that visits every run once per cell,
    // not binning a few times slower.
    void bigDriveBinsQuickly()
    {
        DriveMap map;
        MapRuns whole{MapWholeFile, {}};
        MapRuns fragmented{MapFragmentedFile, {}};
        for (quint64 i = 0; i < 500'000; ++i)
            (i % 3 ? whole : fragmented).runs.append({i * 1000, 600});
        map.add(whole);
        map.add(fragmented);
        QElapsedTimer clock;
        clock.start();
        QCOMPARE(map.cells(20'000).size(), 20'000);
        const qint64 ms = clock.elapsed();
        qInfo("binned in %lld ms", qlonglong(ms));
        QVERIFY2(ms < 2000, qPrintable(QString::number(ms)));
    }

    // DFR-0074: a local user who can write to the drive chooses how many
    // runs there are, so the map's memory must not grow with them. Four
    // million runs held as they came would take 64 MiB. Where a sanitizer
    // replaces malloc, mallinfo2 counts nothing and this proves nothing.
    void manyRunsHoldBoundedMemory()
    {
        DriveMap map;
        const size_t before = mallinfo2().uordblks;
        for (quint64 batch = 0; batch < 40; ++batch) {
            MapRuns some{MapFragmentedFile, {}};
            some.runs.reserve(100'000);
            for (quint64 i = 0; i < 100'000; ++i)
                some.runs.append({(batch * 100'000 + i) * 2, 1});
            map.add(some);
        }
        const size_t held = mallinfo2().uordblks - before;
        QVERIFY2(held < 16 * 1024 * 1024, qPrintable(QString::number(held)));
        QCOMPARE(map.blocks(), quint64(8'000'000 - 1));
        // Half of every block pair is fragmented, half unnamed: a tie,
        // which goes to the later kind.
        QCOMPARE(map.cells(4), QList<CellState>(4, CellState::Fragmented));
    }

    // DFR-0018 INV-1: a move frees the blocks it left and fills the ones
    // it reached; a file that ends in several pieces shows as fragmented
    // where it landed, and an earlier file that ended whole stays whole.
    // One block per cell, so each cell is one tally.
    void movesChangeCells()
    {
        using enum CellState;
        DriveMap map;
        map.add(runs(MapFragmentedFile, {{0, 2}, {4, 2}}));
        map.add(runs(MapFree, {{2, 2}, {6, 6}}));
        QCOMPARE(map.blocksPerCell(12), quint64(1));

        // The first file: blocks 0-1 go to 6-7, and it ends in one piece.
        map.move({{{0, 2}}, {{6, 2}}});
        QCOMPARE(map.cells(12), (QList<CellState>{Free, Free, Free, Free, Fragmented, Fragmented,
                                                  Whole, Whole, Free, Free, Free, Free}));
        map.endTurn(true);

        // The second: blocks 4-5 go to 8-9, and it ends in two pieces.
        map.move({{{4, 2}}, {{8, 2}}});
        QCOMPARE(map.cells(12).mid(8, 2), (QList<CellState>{Whole, Whole}));
        map.endTurn(false);
        QCOMPARE(map.cells(12), (QList<CellState>{Free, Free, Free, Free, Free, Free, Whole,
                                                  Whole, Fragmented, Fragmented, Free, Free}));
        // A turn's end with nothing landed changes nothing.
        map.endTurn(false);
        QCOMPARE(map.cells(12).mid(6, 4), (QList<CellState>{Whole, Whole, Fragmented, Fragmented}));
    }

    // DFR-0018 INV-2: the map can be older than the drive. A move from
    // blocks it holds as a whole file's, to blocks it holds as the
    // filesystem's, in no run, and past its end, still shows the blocks
    // freed and filled, and wraps no tally below zero.
    void moveOnAStaleMap()
    {
        using enum CellState;
        DriveMap map;
        map.add(runs(MapWholeFile, {{0, 2}}));
        map.add(runs(MapFilesystem, {{2, 2}}));
        map.add(runs(MapFree, {{6, 2}}));  // blocks 4-5 are in no run
        map.move({{{0, 2}}, {{2, 1}, {4, 1}}});
        QCOMPARE(map.cells(8), (QList<CellState>{Free, Free, Whole, Filesystem, Whole,
                                                 Filesystem, Free, Free}));
        map.move({{{6, 2}}, {{8, 2}}});
        QCOMPARE(map.blocks(), quint64(10));
        QCOMPARE(map.cells(10).mid(6), (QList<CellState>{Free, Free, Whole, Whole}));
        map.endTurn(false);
        QCOMPARE(map.cells(10), (QList<CellState>{Free, Free, Fragmented, Filesystem, Fragmented,
                                                  Filesystem, Free, Free, Fragmented, Fragmented}));
    }

    // A turn's landed blocks follow the bins when the map outgrows them.
    void landedBlocksSurviveWiderBins()
    {
        DriveMap map;
        const quint64 size = quint64(DriveMap::kBinsMax);
        map.add(runs(MapFree, {{0, size}}));
        map.move({{}, {{0, size / 2}}});
        // Past the end: the bins double in width.
        map.move({{}, {{size, size}}});
        map.endTurn(false);
        QCOMPARE(map.cells(4), (QList<CellState>{CellState::Fragmented, CellState::Free,
                                                 CellState::Fragmented, CellState::Fragmented}));
    }
};

QTEST_GUILESS_MAIN(TestDriveMap)
#include "tst_drivemap.moc"
