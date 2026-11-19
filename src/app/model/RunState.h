// RunState: one defrag of a folder, from start to end (S4, S7). Runs the
// engine's defrag, asking it for the drive's map first unless it holds
// one, applies each move to the map, and counts what happened to each file
// (docs/specs/DFR-0018-window-folder-defrag.md § 4.2; the records are
// docs/specs/DFR-0002-engine-protocol.md § 4.6 and § 4.12).
#pragma once

#include "engine/EngineProcess.h"
#include "engine/Records.h"
#include "model/DriveMap.h"
#include "model/Messages.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace defragr::model {

// The folder as defrag's argument (§ 4.3): relative to the mount point,
// "" for the mount point itself. Nothing when `chosen` is not that folder
// or one below it. Compared by name: no link is read, so a link out of
// the drive is the engine's to refuse.
std::optional<QByteArray> folderOnDrive(const QByteArray &mountPoint, const QString &chosen);

class RunState : public QObject {
    Q_OBJECT

public:
    enum class Stage {
        Idle,        // nothing has run
        ReadingMap,  // the engine is sending the drive's map, before it defrags
        Defragging,
        Done,        // the defrag finished OK
        Failed,      // it did not; problem() says how
    };

    // What happened to the files in two or more pieces.
    struct Counts {
        quint64 joined = 0;      // moved into fewer pieces
        quint64 left = 0;        // no free space held them in fewer pieces
        quint64 skipped = 0;
        quint64 blocksMoved = 0;
        bool operator==(const Counts &) const = default;
    };

    // How many skipped files are kept to show. A local user who can write
    // to the drive chooses how many there are; the count goes on past it.
    static constexpr qsizetype kSkippedListed = 1000;

    // launcher is the program that gains admin rights; tests pass a fake.
    explicit RunState(QString launcher = QStringLiteral("pkexec"), QObject *parent = nullptr);

    // A map of the drive mounted there, to use instead of reading one.
    // Ignored while a run is going.
    void setMap(const QByteArray &mountPoint, const DriveMap &map);
    bool hasMapOf(const QByteArray &mountPoint) const;

    // Defrags `folder`, written as folderOnDrive gives it, on the drive
    // mounted at mountPoint; ends in ended(). Ignored while a run is going.
    void start(const QByteArray &mountPoint, const QByteArray &folder);
    bool isRunning() const { return m_stage == Stage::ReadingMap || m_stage == Stage::Defragging; }

    Stage stage() const { return m_stage; }
    const QByteArray &mountPoint() const { return m_mountPoint; }
    const QByteArray &folder() const { return m_folder; }
    // The drive's map, with every move so far; empty when none is held.
    const DriveMap &map() const { return m_map; }
    const Counts &counts() const { return m_counts; }
    // How far the defrag has got; nothing until the engine has counted
    // the folder's work.
    const std::optional<engine::DefragProgress> &work() const { return m_work; }
    // The file whose turn ended last, relative to the mount point.
    const QByteArray &latestPath() const { return m_latestPath; }
    const QList<engine::FileSkipped> &skipped() const { return m_skipped; }
    // Why the run failed; empty unless the stage is Failed.
    const Message &problem() const { return m_problem; }

signals:
    void stageChanged();
    // While the map is read: names the engine has read, of about `estimate`.
    void mapProgressed(quint64 names, quint64 estimate);
    // A move, after map() has taken it.
    void blocksMoved(const defragr::engine::BlocksMoved &move);
    // The counts, the latest path or the map changed.
    void changed();
    // The stage is now Done or Failed.
    void ended();

private:
    void collect(const QList<engine::Record> &records);
    void engineEnded(const engine::RunOutcome &outcome);
    void setStage(Stage stage);
    void fail(const Message &problem);

    engine::EngineProcess m_engine;
    Stage m_stage = Stage::Idle;
    QByteArray m_mountPoint;
    QByteArray m_folder;
    DriveMap m_map;
    QByteArray m_mapMount;  // the drive m_map is of; empty when it holds none
    DriveMap m_incomingMap;  // the map so far, until the engine says it is whole
    Counts m_counts;
    std::optional<engine::DefragProgress> m_work;
    QByteArray m_latestPath;
    QList<engine::FileSkipped> m_skipped;
    Message m_problem;
};

// What a run did so far, as short sentences: files joined up, files left
// as they were, files skipped. A count of none gets no sentence, except
// that a run with nothing to say says so.
QStringList countLines(const RunState::Counts &counts);

// How far a defrag has got, 0 to 100: by blocks, so a big file counts
// for more than a small one. 100 with no work to do, and never more.
int percentDone(const engine::DefragProgress &work);
// The same as a sentence naming the files done and to do.
QString workLine(const engine::DefragProgress &work);

}  // namespace defragr::model
