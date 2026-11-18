// A fake engine's pause, held by the test instead of timed, so a busy
// machine cannot reorder the two. The fakes' side is fakes/hold.sh: the
// fake creates `sent` in this folder, and waits for the test to create
// `go`. Removing the folder also ends the wait, so a test that fails
// first leaves no fake waiting.
#pragma once

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

class FakeHold {
public:
    FakeHold() : m_dir(QDir::current().filePath(QStringLiteral("hold-XXXXXX")))
    {
        if (m_dir.isValid())
            qputenv("DEFRAGR_TEST_HOLD", QFile::encodeName(m_dir.path()));
    }
    ~FakeHold() { qunsetenv("DEFRAGR_TEST_HOLD"); }
    FakeHold(const FakeHold &) = delete;
    FakeHold &operator=(const FakeHold &) = delete;

    bool isValid() const { return m_dir.isValid(); }
    // The fake has created `sent`.
    bool sent() const { return QFile::exists(m_dir.filePath(QStringLiteral("sent"))); }
    // Lets the fake go on.
    bool release()
    {
        QFile go(m_dir.filePath(QStringLiteral("go")));
        return go.open(QIODevice::WriteOnly);
    }

private:
    QTemporaryDir m_dir;
};
