// The host half of the image_report test (test-report.sh). Decodes the
// engine's report on the fragmented image with the window's decoder and
// compares it with filefrag's reference, the named one for extent counts
// (docs/design.md § How it is tested). Also checks INV-12's stream: a
// report on a sub-folder ends NOT_A_MOUNT and names no file; and INV-14's
// first and last REPORT_PROGRESS.
//
// Usage: report-check REFERENCE BLOCKS REPORT SUBDIR INODES
//   REFERENCE  describe_files lines: EXTENTS SHA256 PATH
//   BLOCKS     lines: BLOCKS PATH, each file's size in 4 KiB blocks
//   REPORT     the engine's output for `report MOUNT`
//   SUBDIR     the engine's output for `report MOUNT/deep`
//   INODES     one line: ESTIMATE NAMES, the drive's used inodes before
//              the report and every name below MOUNT
// Prints one "ok" or "FAIL" line per check; exits 1 on any failure.
#include "engine/Records.h"

#include <QFile>
#include <QMap>

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
    if (argc != 6) {
        std::fprintf(stderr, "usage: report-check REFERENCE BLOCKS REPORT SUBDIR INODES\n");
        return 2;
    }
    checkReport(readFile(argv[1]), readFile(argv[2]), readFile(argv[3]));
    checkSubdir(readFile(argv[4]));
    checkProgress(readFile(argv[5]), readFile(argv[3]));
    return failures == 0 ? 0 : 1;
}
