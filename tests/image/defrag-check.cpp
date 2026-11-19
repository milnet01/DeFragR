// The host half of the image_defrag test (test-defrag.sh). Decodes the
// engine's output with the window's decoder and compares it with what
// filefrag saw in the test machine before and after: INV-20's records,
// INV-23's skips, INV-25's map, and INV-22's probe record on a CPU
// without SSE4.2 (docs/specs/DFR-0002-engine-protocol.md).
//
// Usage:
//   defrag-check defrag STREAM FOLDER REF_BEFORE REF_AFTER EXT_BEFORE EXT_AFTER MAP [SKIPPED]...
//     STREAM      the engine's output for `defrag MOUNT FOLDER`, or for
//                 `defrag MOUNT FOLDER map`
//     FOLDER      that folder; "" for the whole drive
//     REF_*       describe_files lines, EXTENTS SHA256 PATH, before and after
//     EXT_*       lines LOGICAL PHYSICAL LENGTH PATH, in blocks, one per
//                 extent filefrag -v lists, before and after
//     MAP         the engine's output for `report MOUNT` just before a
//                 defrag asked for the map; "-" for one not asked
//     SKIPPED     each path the run must skip, and no other
//   defrag-check probe STREAM MOUNT
//     STREAM      the engine's output for `probe` on a CPU without SSE4.2
//     MOUNT       an ext4 drive there that would otherwise allow defrag
// Prints one "ok" or "FAIL" line per check; exits 1 on any failure.
#include "engine/Records.h"

#include <QFile>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace defragr::engine;

namespace {

int failures = 0;

void check(bool passed, const QString &what)
{
    std::printf("%s %s\n", passed ? "ok  " : "FAIL", qPrintable(what));
    if (!passed)
        ++failures;
}

QByteArray readFile(const char *path)
{
    QFile file(QString::fromLocal8Bit(path));
    if (!file.open(QIODevice::ReadOnly)) {
        std::printf("FAIL cannot read %s\n", path);
        std::exit(1);
    }
    return file.readAll();
}

struct Decoded {
    QList<Record> records;
    std::optional<StreamError> error;
};

Decoded decode(const QByteArray &bytes)
{
    RecordDecoder decoder;
    decoder.feed(bytes);
    decoder.finish();
    return {decoder.takeRecords(), decoder.error()};
}

// Each describe_files line's extent count, keyed by its path.
QMap<QByteArray, quint32> countByPath(const QByteArray &text)
{
    QMap<QByteArray, quint32> map;
    for (const QByteArray &line : text.split('\n')) {
        const QList<QByteArray> fields = line.split(' ');
        if (fields.size() == 3)
            map.insert(fields[2], fields[0].toUInt());
    }
    return map;
}

// Each file's blocks on the drive, in the file's order, from filefrag -v.
QMap<QByteArray, QList<quint64>> blocksByPath(const QByteArray &text)
{
    QMap<QByteArray, QList<std::array<quint64, 3>>> extents;
    for (const QByteArray &line : text.split('\n')) {
        const QList<QByteArray> fields = line.split(' ');
        if (fields.size() == 4)
            extents[fields[3]].append({fields[0].toULongLong(), fields[1].toULongLong(),
                                       fields[2].toULongLong()});
    }
    QMap<QByteArray, QList<quint64>> blocks;
    for (auto it = extents.begin(); it != extents.end(); ++it) {
        std::ranges::sort(*it);  // by logical block
        QList<quint64> &out = blocks[it.key()];
        for (const auto &[logical, physical, length] : *it)
            for (quint64 i = 0; i < length; ++i)
                out.append(physical + i);
    }
    return blocks;
}

QList<quint64> blocksOf(const QList<MapRun> &runs)
{
    QList<quint64> out;
    for (const MapRun &run : runs)
        for (quint64 i = 0; i < run.length; ++i)
            out.append(run.start + i);
    return out;
}

// The blocks each kind of MAP_RUNS names in the stream, up to its
// MAP_DONE if it has one. Sets, since a run is cut wherever a record
// fills.
QMap<quint32, QSet<quint64>> mapBlocks(const QList<Record> &records)
{
    QMap<quint32, QSet<quint64>> blocks;
    for (const Record &record : records) {
        if (std::holds_alternative<MapDone>(record))
            break;
        if (const auto *runs = std::get_if<MapRuns>(&record))
            for (const quint64 block : blocksOf(runs->runs))
                blocks[runs->kind].insert(block);
    }
    return blocks;
}

// INV-25: a defrag asked for the map sends, before anything else, what a
// report's map says; one not asked sends none of it.
void checkMap(const Decoded &run, const char *reportPath, const QString &in)
{
    qsizetype done = -1, lastOfMap = -1, firstOfFiles = -1;
    int dones = 0;
    for (qsizetype i = 0; i < run.records.size(); ++i) {
        const Record &record = run.records[i];
        if (std::holds_alternative<MapDone>(record)) {
            done = i;
            ++dones;
        } else if (std::holds_alternative<MapRuns>(record)
                   || std::holds_alternative<ReportProgress>(record)) {
            lastOfMap = i;
        } else if (firstOfFiles < 0
                   && (std::holds_alternative<BlocksMoved>(record)
                       || std::holds_alternative<FileDefragged>(record)
                       || std::holds_alternative<FileSkipped>(record))) {
            firstOfFiles = i;
        }
    }
    if (std::strcmp(reportPath, "-") == 0) {
        check(dones == 0 && lastOfMap < 0, "no map is sent unasked" + in);
        return;
    }
    check(dones == 1, "MAP_DONE comes once" + in);
    check(lastOfMap >= 0 && lastOfMap < done, "the map comes before MAP_DONE" + in);
    check(firstOfFiles < 0 || firstOfFiles > done, "the files come after MAP_DONE" + in);
    const auto *first = run.records.size() > 1 ? std::get_if<ReportProgress>(&run.records[1])
                                               : nullptr;
    check(first && first->names == 0, "REPORT_PROGRESS with no names comes after HELLO" + in);

    const Decoded report = decode(readFile(reportPath));
    check(!report.error, "the report taken before decodes" + in);
    const auto wanted = mapBlocks(report.records);
    const auto sent = mapBlocks(run.records);
    check(!wanted.value(MapFragmentedFile).isEmpty() && !wanted.value(MapFree).isEmpty(),
          "the report's map has fragmented files and free space" + in);
    for (const quint32 kind : {MapFree, MapFilesystem, MapWholeFile, MapFragmentedFile})
        check(sent.value(kind) == wanted.value(kind),
              QStringLiteral("the map's blocks of kind %1 are the report's").arg(kind) + in);
}

int checkDefrag(char **argv, int argc)
{
    const Decoded run = decode(readFile(argv[0]));
    const QByteArray folder = argv[1];
    const QByteArray prefix = folder.isEmpty() ? QByteArray() : folder + '/';
    const auto countBefore = countByPath(readFile(argv[2]));
    const auto countAfter = countByPath(readFile(argv[3]));
    const auto blocksBefore = blocksByPath(readFile(argv[4]));
    const auto blocksAfter = blocksByPath(readFile(argv[5]));
    QSet<QByteArray> mustSkip;
    for (int i = 7; i < argc; ++i)
        mustSkip.insert(argv[i]);
    const QString in = QStringLiteral(" (defrag of \"%1\")").arg(QString::fromLatin1(folder));

    check(!run.error, "the stream decodes" + in);
    check(!run.records.isEmpty() && std::holds_alternative<Hello>(run.records.first())
              && std::get<Hello>(run.records.first()).command == CommandDefrag
              && std::get<Hello>(run.records.first()).blockSize == 4096,
          "HELLO is defrag's, in 4 KiB blocks" + in);
    const auto *finished = run.records.isEmpty() ? nullptr
                                                 : std::get_if<Finished>(&run.records.last());
    check(finished && finished->result == ResultOk, "FINISHED is OK" + in);
    checkMap(run, argv[6], in);

    // Each file's BLOCKS_MOVED come straight before its own record.
    QList<MapRun> from, to;
    QSet<QByteArray> defragged, skipped;
    for (const Record &record : run.records) {
        if (const auto *moved = std::get_if<BlocksMoved>(&record)) {
            from += moved->from;
            to += moved->to;
            continue;
        }
        if (const auto *s = std::get_if<FileSkipped>(&record)) {
            skipped.insert(s->path);
            from.clear();
            to.clear();
            continue;
        }
        const auto *f = std::get_if<FileDefragged>(&record);
        if (!f)
            continue;
        const QString name = QString::fromLatin1(f->path);
        defragged.insert(f->path);
        check(f->extentsBefore == countBefore.value(f->path),
              name + ": extents_before is filefrag's count before");
        check(f->extentsAfter == countAfter.value(f->path),
              name + ": extents_after is filefrag's count after");
        check(f->movedBlocks > 0 && f->extentsAfter < f->extentsBefore,
              name + ": moved, into fewer pieces");
        const QList<quint64> left = blocksOf(from), went = blocksOf(to);
        check(left == blocksBefore.value(f->path),
              name + ": the from runs are its blocks before, in order");
        check(went == blocksAfter.value(f->path),
              name + ": the to runs are its blocks after, in order");
        check(f->movedBlocks == quint64(left.size()), name + ": moved_blocks counts them");
        from.clear();
        to.clear();
    }
    check(from.isEmpty() && to.isEmpty(), "no BLOCKS_MOVED is left without its file" + in);

    for (auto it = countBefore.begin(); it != countBefore.end(); ++it) {
        const QString name = QString::fromLatin1(it.key());
        const bool inFolder = it.key().startsWith(prefix);
        const bool wanted = inFolder && it.value() >= 2 && !mustSkip.contains(it.key());
        check(defragged.contains(it.key()) == wanted,
              name + (wanted ? ": defragged" : ": not defragged") + in);
        if (!inFolder || mustSkip.contains(it.key()))
            check(blocksAfter.value(it.key()) == blocksBefore.value(it.key()),
                  name + ": its blocks did not move" + in);
    }
    check(skipped == mustSkip, "exactly the files that must be skipped are" + in);
    return failures == 0 ? 0 : 1;
}

int checkProbe(char **argv)
{
    const Decoded run = decode(readFile(argv[0]));
    check(!run.error, "the probe stream decodes");
    int found = 0;
    for (const Record &record : run.records) {
        const auto *drive = std::get_if<Drive>(&record);
        if (!drive || drive->mountPoint != argv[1])
            continue;
        ++found;
        check(drive->verdict == VerdictReportOnly && drive->reason == ReasonOldCpu,
              QStringLiteral("the drive is report only, for the old CPU"));
    }
    check(found == 1, QStringLiteral("probe lists the drive once"));
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc >= 9 && std::strcmp(argv[1], "defrag") == 0)
        return checkDefrag(argv + 2, argc - 2);
    if (argc == 4 && std::strcmp(argv[1], "probe") == 0)
        return checkProbe(argv + 2);
    std::fprintf(stderr, "usage: see the header of tests/image/defrag-check.cpp\n");
    return 2;
}
