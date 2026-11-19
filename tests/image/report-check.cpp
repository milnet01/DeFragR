// The host half of the image_report test (test-report.sh). Decodes the
// engine's report on the fragmented image with the window's decoder and
// compares it with filefrag's reference, the named one for extent counts
// (docs/design.md § How it is tested). Also checks INV-12's stream: a
// report on a sub-folder ends NOT_A_MOUNT and names no file; INV-14's
// first and last REPORT_PROGRESS; and INV-17's map runs.
//
// Usage: report-check REFERENCE BLOCKS REPORT SUBDIR INODES SPACE EXTENTS
//   REFERENCE  describe_files lines: EXTENTS SHA256 PATH
//   BLOCKS     lines: BLOCKS PATH, each file's size in 4 KiB blocks
//   REPORT     the engine's output for `report MOUNT`
//   SUBDIR     the engine's output for `report MOUNT/deep`
//   INODES     one line: ESTIMATE NAMES, the drive's used inodes before
//              the report and every name below MOUNT
//   SPACE      one line: FREE TOTAL, the drive's free blocks before the
//              report and the disk's size, in 4 KiB blocks
//   EXTENTS    lines: START LENGTH PATH, each extent filefrag -v lists
// Prints one "ok" or "FAIL" line per check; exits 1 on any failure.
#include "engine/Records.h"

#include <QFile>
#include <QMap>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

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

// Each line's first field as a number, keyed by its last field.
QMap<QByteArray, quint64> numberByPath(const QByteArray &text)
{
    QMap<QByteArray, quint64> map;
    for (const QByteArray &line : text.split('\n')) {
        const QList<QByteArray> fields = line.split(' ');
        if (fields.size() >= 2)
            map.insert(fields.last(), fields.first().toULongLong());
    }
    return map;
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

template <typename T>
QList<T> recordsOf(const QList<Record> &records)
{
    QList<T> found;
    for (const Record &record : records)
        if (const T *one = std::get_if<T>(&record))
            found.append(*one);
    return found;
}

void checkReport(const QByteArray &referenceText, const QByteArray &blocksText,
                 const QByteArray &stream)
{
    const QMap<QByteArray, quint64> reference = numberByPath(referenceText);
    const QMap<QByteArray, quint64> blocks = numberByPath(blocksText);
    check(!reference.isEmpty(), QStringLiteral("the reference lists files"));

    const Decoded run = decode(stream);
    check(!run.error, QStringLiteral("the report decodes with no error"));
    const QList<Hello> hello = recordsOf<Hello>(run.records);
    check(hello.size() == 1 && hello[0].command == CommandReport && hello[0].blockSize == 4096,
          QStringLiteral("HELLO is a report with block size 4096"));
    const QList<Finished> finished = recordsOf<Finished>(run.records);
    check(finished.size() == 1 && finished[0].result == ResultOk,
          QStringLiteral("the report finishes OK"));
    check(recordsOf<FileSkipped>(run.records).isEmpty(), QStringLiteral("no file is skipped"));

    QMap<QByteArray, FragmentedFile> reported;
    for (const FragmentedFile &file : recordsOf<FragmentedFile>(run.records)) {
        check(!reported.contains(file.path),
              QStringLiteral("%1 is reported once").arg(QString::fromUtf8(file.path)));
        reported.insert(file.path, file);
    }

    quint64 extents = 0;
    int fragmented = 0;
    for (auto it = reference.cbegin(); it != reference.cend(); ++it) {
        const QString path = QString::fromUtf8(it.key());
        extents += it.value();
        if (it.value() < 2) {
            check(!reported.contains(it.key()),
                  QStringLiteral("%1, in %2 piece(s), is not listed").arg(path).arg(it.value()));
            continue;
        }
        ++fragmented;
        const FragmentedFile file = reported.value(it.key());
        check(file.extents == it.value(), QStringLiteral("%1: %2 extents, filefrag says %3")
                                              .arg(path).arg(file.extents).arg(it.value()));
        check(file.sizeBlocks == blocks.value(it.key()),
              QStringLiteral("%1: %2 blocks, its size says %3")
                  .arg(path).arg(file.sizeBlocks).arg(blocks.value(it.key())));
    }
    check(reported.size() == fragmented,
          QStringLiteral("%1 files listed, filefrag finds %2 in two or more pieces")
              .arg(reported.size()).arg(fragmented));

    const QList<ReportSummary> summary = recordsOf<ReportSummary>(run.records);
    check(summary.size() == 1, QStringLiteral("one REPORT_SUMMARY"));
    if (summary.size() == 1) {
        quint64 fileBlocks = 0;
        for (const quint64 count : blocks)
            fileBlocks += count;
        const ReportSummary &s = summary[0];
        check(s.files == quint64(reference.size()),
              QStringLiteral("summary: %1 files, the reference %2").arg(s.files).arg(reference.size()));
        check(s.extents == extents,
              QStringLiteral("summary: %1 extents, filefrag %2").arg(s.extents).arg(extents));
        check(s.fileBlocks == fileBlocks,
              QStringLiteral("summary: %1 blocks, the sizes %2").arg(s.fileBlocks).arg(fileBlocks));
    }
}

// INV-14: REPORT_PROGRESS straight after HELLO with names 0, and straight
// before REPORT_SUMMARY with every name; both carry the used inodes.
void checkProgress(const QByteArray &inodesText, const QByteArray &stream)
{
    const QList<QByteArray> fields = inodesText.trimmed().split(' ');
    check(fields.size() == 2, QStringLiteral("INODES is two numbers"));
    if (fields.size() != 2)
        return;
    const quint64 estimate = fields[0].toULongLong();
    const quint64 names = fields[1].toULongLong();
    check(estimate > 0 && names > 0, QStringLiteral("the image has used inodes and names"));

    const QList<Record> records = decode(stream).records;
    const auto progressAt = [&](qsizetype i) -> std::optional<ReportProgress> {
        if (i < 0 || i >= records.size())
            return std::nullopt;
        if (const auto *progress = std::get_if<ReportProgress>(&records[i]))
            return *progress;
        return std::nullopt;
    };
    const std::optional<ReportProgress> first = progressAt(1);
    check(first && *first == (ReportProgress{0, estimate}),
          QStringLiteral("REPORT_PROGRESS after HELLO: names 0, estimate %1, got %2 %3")
              .arg(estimate).arg(first ? first->names : 0).arg(first ? first->estimate : 0));
    // REPORT_SUMMARY and FINISHED are the last two records (INV-8).
    const std::optional<ReportProgress> last = progressAt(records.size() - 3);
    check(last && *last == (ReportProgress{names, estimate}),
          QStringLiteral("REPORT_PROGRESS before REPORT_SUMMARY: names %1, estimate %2, got "
                         "%3 %4")
              .arg(names).arg(estimate).arg(last ? last->names : 0)
              .arg(last ? last->estimate : 0));
}

// INV-17: the map's runs, block by block, against statfs and filefrag -v.
void checkMap(const QByteArray &referenceText, const QByteArray &spaceText,
              const QByteArray &extentsText, const QByteArray &stream)
{
    const QList<QByteArray> space = spaceText.trimmed().split(' ');
    check(space.size() == 2, QStringLiteral("SPACE is two numbers"));
    if (space.size() != 2)
        return;
    const quint64 free = space[0].toULongLong();
    const quint64 total = space[1].toULongLong();
    check(free > 0 && total > free, QStringLiteral("the drive has free and used blocks"));

    // Each block's kind in the engine's map; 0 where no run lies.
    QList<quint32> kindOf(qsizetype(total), 0);
    bool inside = true;
    bool apart = true;
    qsizetype freeRuns = 0;
    for (const MapRuns &batch : recordsOf<MapRuns>(decode(stream).records)) {
        if (batch.kind == MapFree)
            freeRuns += batch.runs.size();
        for (const MapRun &run : batch.runs) {
            if (run.start > total || run.length > total - run.start) {
                inside = false;
                continue;
            }
            for (quint64 b = run.start; b < run.start + run.length; ++b) {
                apart = apart && kindOf[qsizetype(b)] == 0;
                kindOf[qsizetype(b)] = batch.kind;
            }
        }
    }
    check(inside, QStringLiteral("every run lies inside the disk's %1 blocks").arg(total));
    check(apart, QStringLiteral("no block lies in two runs"));
    const auto freeBlocks = quint64(std::count(kindOf.cbegin(), kindOf.cend(), MapFree));
    check(freeBlocks == free,
          QStringLiteral("free runs cover %1 blocks, statfs says %2").arg(freeBlocks).arg(free));
    // FSMAP_PER_CALL in src/engine/extents.asm: more runs need a second call.
    check(freeRuns > 64,
          QStringLiteral("free space is in %1 runs, more than one GETFSMAP call holds")
              .arg(freeRuns));
    check(total > 0 && kindOf[0] == MapFilesystem,
          QStringLiteral("block 0, the superblock's, is in a filesystem run"));

    // Every file's blocks, as kind 3 when filefrag counts one piece,
    // and otherwise as kind 5 when its 4 KiB blocks average 16 MiB or
    // more a piece and kind 4 when they do not (INV-28); every other
    // block is no file's.
    const QMap<QByteArray, quint64> pieces = numberByPath(referenceText);
    QMap<QByteArray, quint64> blocksOf;
    for (const QByteArray &line : extentsText.split('\n')) {
        const QList<QByteArray> fields = line.split(' ');
        if (fields.size() >= 3)
            blocksOf[fields.mid(2).join(' ')] += fields[1].toULongLong();
    }
    const auto kindFor = [&](const QByteArray &path) -> quint32 {
        if (pieces.value(path) < 2)
            return MapWholeFile;
        return blocksOf.value(path) * 4096 >= pieces.value(path) * LargePieceBytes
                   ? MapLargePieceFile
                   : MapFragmentedFile;
    };
    QList<quint32> expected(qsizetype(total), 0);
    for (const QByteArray &line : extentsText.split('\n')) {
        const QList<QByteArray> fields = line.split(' ');
        if (fields.size() < 3)
            continue;
        const quint64 start = fields[0].toULongLong();
        const quint64 length = fields[1].toULongLong();
        const quint32 kind = kindFor(fields.mid(2).join(' '));
        for (quint64 b = start; b < start + length && b < total; ++b)
            expected[qsizetype(b)] = kind;
    }
    for (const quint32 kind : {MapWholeFile, MapFragmentedFile, MapLargePieceFile})
        check(expected.contains(kind),
              QStringLiteral("filefrag shows a file of kind %1").arg(kind));
    const auto isFile = [](quint32 kind) {
        return kind == MapWholeFile || kind == MapFragmentedFile || kind == MapLargePieceFile;
    };
    qsizetype wrong = 0;
    qsizetype first = -1;
    for (qsizetype b = 0; b < kindOf.size(); ++b) {
        const quint32 got = isFile(kindOf[b]) ? kindOf[b] : 0;
        if (got != expected[b]) {
            ++wrong;
            if (first < 0)
                first = b;
        }
    }
    check(wrong == 0,
          QStringLiteral("file runs match filefrag -v: %1 blocks differ, the first %2")
              .arg(wrong).arg(first));
}

// INV-12: nothing but HELLO and FINISHED NOT_A_MOUNT.
void checkSubdir(const QByteArray &stream)
{
    const Decoded run = decode(stream);
    check(!run.error, QStringLiteral("the sub-folder report decodes with no error"));
    check(run.records.size() == 2 && std::holds_alternative<Hello>(run.records[0])
              && std::holds_alternative<Finished>(run.records[1])
              && std::get<Finished>(run.records[1]).result == ResultNotAMount,
          QStringLiteral("the sub-folder report is HELLO, then FINISHED NOT_A_MOUNT"));
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc != 8) {
        std::fprintf(stderr, "usage: report-check REFERENCE BLOCKS REPORT SUBDIR INODES SPACE "
                             "EXTENTS\n");
        return 2;
    }
    checkReport(readFile(argv[1]), readFile(argv[2]), readFile(argv[3]));
    checkSubdir(readFile(argv[4]));
    checkProgress(readFile(argv[5]), readFile(argv[3]));
    checkMap(readFile(argv[1]), readFile(argv[6]), readFile(argv[7]), readFile(argv[3]));
    return failures == 0 ? 0 : 1;
}
