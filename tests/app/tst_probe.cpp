// The engine's probe against this machine (DFR-0004; the rules are in
// src/engine/probe.asm, the layout in docs/specs/DFR-0002-engine-protocol.md
// § 4.5 and § 4.7). Each fact is checked by a route the engine does not
// take: Qt's QStorageInfo, the C library's statfs, and sysfs's partition
// file. The engine's path comes from DEFRAGR_ENGINE.
#include "engine/Records.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QStorageInfo>
#include <QTest>

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>

using namespace defragr::engine;

namespace {

struct Probe {
    QList<Drive> drives;
    std::optional<StreamError> error;
    int result = -1;
    int exitCode = -1;
};

Probe runProbe()
{
    Probe probe;
    QProcess engine;
    engine.start(qEnvironmentVariable("DEFRAGR_ENGINE"), {QStringLiteral("probe")});
    if (!engine.waitForStarted(10000)) {
        qWarning("engine did not start: %s", qPrintable(engine.errorString()));
        return probe;
    }
    engine.closeWriteChannel();
    if (!engine.waitForFinished(10000) || engine.exitStatus() != QProcess::NormalExit) {
        qWarning("engine did not exit normally");
        return probe;
    }
    RecordDecoder decoder;
    decoder.feed(engine.readAllStandardOutput());
    decoder.finish();
    for (const Record &record : decoder.takeRecords()) {
        if (const auto *drive = std::get_if<Drive>(&record))
            probe.drives.append(*drive);
        else if (const auto *finished = std::get_if<Finished>(&record))
            probe.result = finished->result;
    }
    probe.error = decoder.error();
    probe.exitCode = engine.exitCode();
    return probe;
}

quint64 deviceKey(quint32 major, quint32 minor)
{
    return (quint64(major) << 32) | minor;
}

// Each mounted block device, by device number, with the name Qt gives it.
// A squashfs mount is left out, as probe.asm's rules leave it out: it is a
// snap package's image, never a drive.
QHash<quint64, QByteArray> mountedBlockDevices()
{
    QHash<quint64, QByteArray> devices;
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        const QByteArray device = volume.device();
        struct stat node {};
        if (volume.fileSystemType() == "squashfs" || !device.startsWith("/dev/")
            || stat(device.constData(), &node) != 0
            || !S_ISBLK(node.st_mode))
            continue;
        devices.insert(deviceKey(major(node.st_rdev), minor(node.st_rdev)), device);
    }
    return devices;
}

// The device's rotational flag, found through sysfs's partition file
// rather than by trying paths: a partition's flag is its disk's.
std::optional<char> rotational(quint32 major, quint32 minor)
{
    const QString node = QFileInfo(QStringLiteral("/sys/dev/block/%1:%2").arg(major).arg(minor))
                             .canonicalFilePath();
    if (node.isEmpty())
        return std::nullopt;
    QDir disk(node);
    if (QFileInfo::exists(disk.filePath(QStringLiteral("partition"))))
        disk.cdUp();
    QFile flag(disk.filePath(QStringLiteral("queue/rotational")));
    if (!flag.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QByteArray text = flag.readAll().trimmed();
    if (text.size() != 1)
        return std::nullopt;
    return text[0];
}

}  // namespace

class TestProbe : public QObject {
    Q_OBJECT

    Probe m_probe;
    QHash<quint64, QByteArray> m_mountedBefore;  // read just before the probe

private slots:
    void initTestCase()
    {
        QVERIFY2(!qEnvironmentVariable("DEFRAGR_ENGINE").isEmpty(), "DEFRAGR_ENGINE is not set");
        m_mountedBefore = mountedBlockDevices();
        m_probe = runProbe();
        QCOMPARE(m_probe.error, std::nullopt);
        QCOMPARE(m_probe.result, int(ResultOk));
        QCOMPARE(m_probe.exitCode, int(ResultOk));
        qInfo("the probe listed %lld drives", qlonglong(m_probe.drives.size()));
    }

    // § 4.7: the reason is the first rule that applies, and the verdict
    // follows from it. Each drive's own fields say which rules apply.
    void reasonFollowsTheRules()
    {
        if (m_probe.drives.isEmpty())
            QSKIP("this machine has no drives the probe lists");
        for (const Drive &drive : std::as_const(m_probe.drives)) {
            quint16 expected = ReasonNone;
            if (drive.fsType != "ext4")
                expected = ReasonFsNotExt4;
            else if (drive.mountFlags & 1)
                expected = ReasonReadOnly;
            else if (drive.media == MediaSsd)
                expected = ReasonSsd;
            else if (drive.media == MediaUnknown)
                expected = ReasonMediaUnknown;
            QCOMPARE(drive.reason, expected);
            QCOMPARE(drive.verdict,
                     quint8(expected == ReasonNone ? VerdictReportAndDefrag : VerdictReportOnly));
        }
    }

    // Each drive's figures are the system's own.
    void drivesMatchTheSystem()
    {
        if (m_probe.drives.isEmpty())
            QSKIP("this machine has no drives the probe lists");
        QSet<QByteArray> mountPoints;
        QSet<quint64> devices;
        for (const Drive &drive : std::as_const(m_probe.drives)) {
            const QByteArray &path = drive.mountPoint;
            QVERIFY2(!mountPoints.contains(path), path.constData());
            QVERIFY2(!devices.contains(deviceKey(drive.devMajor, drive.devMinor)), path.constData());
            mountPoints.insert(path);
            devices.insert(deviceKey(drive.devMajor, drive.devMinor));

            struct statvfs vfs {};
            QCOMPARE(statvfs(path.constData(), &vfs), 0);
            QCOMPARE(drive.totalBlocks * drive.blockSize, quint64(vfs.f_blocks) * vfs.f_frsize);
            QCOMPARE(bool(drive.mountFlags & 1), bool(vfs.f_flag & ST_RDONLY));

            const QStorageInfo volume(QFile::decodeName(path));
            QCOMPARE(volume.rootPath(), QFile::decodeName(path));
            QCOMPARE(drive.fsType, volume.fileSystemType());

            const std::optional<char> flag = rotational(drive.devMajor, drive.devMinor);
            const quint8 media = flag == '0' ? MediaSsd : flag == '1' ? MediaSpinning : MediaUnknown;
            QCOMPARE(drive.media, media);
        }
    }

    // S1: every mounted block device appears once. Only a device mounted
    // both before the probe and now counts: one mounted or unmounted
    // meanwhile, as by an automount, may or may not be in the probe.
    void listsEveryDrive()
    {
        QSet<quint64> listed;
        for (const Drive &drive : std::as_const(m_probe.drives))
            listed.insert(deviceKey(drive.devMajor, drive.devMinor));
        int checked = 0;
        const QHash<quint64, QByteArray> mountedAfter = mountedBlockDevices();
        for (auto it = mountedAfter.cbegin(); it != mountedAfter.cend(); ++it) {
            if (!m_mountedBefore.contains(it.key()))
                continue;
            QVERIFY2(listed.contains(it.key()), it.value().constData());
            ++checked;
        }
        if (checked == 0)
            QSKIP("Qt sees no mounted block device here");
    }
};

QTEST_GUILESS_MAIN(TestProbe)
#include "tst_probe.moc"
