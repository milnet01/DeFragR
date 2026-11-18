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
        QVERIFY2(clock.elapsed() < 500, qPrintable(QString::number(clock.elapsed())));
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
};

QTEST_GUILESS_MAIN(TestDriveMap)
#include "tst_drivemap.moc"
