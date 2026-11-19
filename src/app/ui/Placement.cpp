#include "ui/Placement.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <utility>

namespace defragr::ui {

namespace {

const QString kSizeKey = QStringLiteral("window/size");
const QString kPositionKey = QStringLiteral("window/position");

// Each call to KWin waits at most this long; D-Bus's own default is 25 s,
// and the window waits on it as it closes.
constexpr int kKWinTimeoutMs = 2000;
// How long KWin's answer may take to reach the journal: measured on
// Plasma 6 at about 10 ms, and 600 ms on a session's first query.
constexpr int kAnswerTimeoutMs = 1500;
constexpr int kJournalPollMs = 50;
// KWin places a new window itself, and a move made before then is undone.
// In a private KWin 6.8 a move one tick after the first Expose was undone,
// and one 100 ms after it held; 300 ms leaves a margin.
constexpr int kMoveAfterExposeMs = 300;
// How long a move script is kept loaded: long enough for KWin to run it.
constexpr int kKeepMoveScriptMs = 3000;

// Qt's own platform, not XDG_SESSION_TYPE: the window can run on X11
// inside a Wayland session, and there it may move itself.
bool onWayland()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

QDBusMessage callKWinScripting(const QString &method, const QVariantList &arguments = {})
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
        QStringLiteral("org.kde.kwin.Scripting"), method);
    call.setArguments(arguments);
    // Block, not the event loop: nothing else of the window's runs
    // meanwhile.
    return QDBusConnection::sessionBus().call(call, QDBus::Block, kKWinTimeoutMs);
}

}  // namespace

// A one-shot KWin script, loaded and started. KWin reads its file and runs
// it later, from its own event loop, so both are kept until this is
// destroyed: a script unloaded at once may never run.
class KWinScript {
public:
    explicit KWinScript(const QString &source)
        : m_file(QDir::tempPath() + QStringLiteral("/defragr-placement-XXXXXX.js"))
    {
        if (!QDBusConnection::sessionBus().isConnected() || !m_file.open()
            || m_file.write(source.toUtf8()) < 0 || !m_file.flush())
            return;
        // A name of its own each time, so a script KWin failed to unload
        // never stops the next one loading.
        const QString name =
            QStringLiteral("defragr_") + QUuid::createUuid().toString(QUuid::Id128);
        const QDBusMessage loaded =
            callKWinScripting(QStringLiteral("loadScript"), {m_file.fileName(), name});
        if (loaded.type() != QDBusMessage::ReplyMessage || loaded.arguments().value(0).toInt() < 0)
            return;
        m_name = name;
        callKWinScripting(QStringLiteral("start"));
    }

    ~KWinScript()
    {
        if (!m_name.isEmpty())
            callKWinScripting(QStringLiteral("unloadScript"), {m_name});
    }

    KWinScript(const KWinScript &) = delete;
    KWinScript &operator=(const KWinScript &) = delete;

    // True when KWin took the script, which says nothing about what it
    // found.
    bool started() const { return !m_name.isEmpty(); }

private:
    QTemporaryFile m_file;
    QString m_name;
};

namespace {

// Where KWin has the window's frame. Only while the window is on screen:
// KWin no longer lists a hidden one.
std::optional<QPoint> frameFromKWin()
{
    const QString journalctl = QStandardPaths::findExecutable(QStringLiteral("journalctl"));
    if (journalctl.isEmpty())
        return {};
    const QString marker =
        QStringLiteral("defragr_frame_") + QUuid::createUuid().toString(QUuid::Id128);
    // Two seconds back, as --since counts whole seconds.
    const QString since =
        QLatin1Char('@') + QString::number(QDateTime::currentSecsSinceEpoch() - 2);
    // Unloaded on return, once it has answered or the wait is over.
    const KWinScript script(Placement::readFrameScript(QCoreApplication::applicationPid(), marker));
    if (!script.started())
        return {};

    QElapsedTimer clock;
    clock.start();
    while (true) {
        QProcess journal;
        journal.start(journalctl, {QStringLiteral("--user"), QStringLiteral("-b"),
                                   QStringLiteral("--since"), since, QStringLiteral("-o"),
                                   QStringLiteral("cat")});
        // Each read gets only what is left, so the whole wait stays bounded.
        const int left = int(std::max<qint64>(100, kAnswerTimeoutMs - clock.elapsed()));
        if (!journal.waitForFinished(left)) {
            journal.kill();
            journal.waitForFinished(100);
            return {};
        }
        const auto frame =
            Placement::frameFromJournal(QString::fromUtf8(journal.readAllStandardOutput()), marker);
        if (frame || clock.elapsed() >= kAnswerTimeoutMs)
            return frame;
        QThread::msleep(kJournalPollMs);
    }
}

// `at`, moved just enough that a window of `size` there lies within
// `area`, or starts at its top left when it is too big to.
QPoint keptWithin(QPoint at, QSize size, const QRect &area)
{
    return {std::max(area.left(), std::min(at.x(), area.right() + 1 - size.width())),
            std::max(area.top(), std::min(at.y(), area.bottom() + 1 - size.height()))};
}

}  // namespace

Placement::Placement(QWidget &window, std::unique_ptr<QSettings> settings)
    : QObject(&window), m_window(window),
      m_settings(settings ? std::move(settings) : userSettings())
{
    restore();
    m_window.installEventFilter(this);
}

Placement::~Placement() = default;

std::unique_ptr<QSettings> Placement::userSettings()
{
    return std::make_unique<QSettings>(QSettings::UserScope, QStringLiteral("DeFragR"),
                                       QStringLiteral("DeFragR"));
}

std::optional<QPoint> Placement::frameFromJournal(const QString &journal, const QString &marker)
{
    for (const QStringView line : QStringView(journal).split(QLatin1Char('\n'))) {
        const qsizetype found = line.indexOf(marker);
        if (found < 0)
            continue;
        const auto words =
            line.mid(found + marker.size()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        bool xRead = false;
        bool yRead = false;
        const int x = words.value(0).toInt(&xRead);
        const int y = words.value(1).toInt(&yRead);
        if (xRead && yRead)
            return QPoint(x, y);
        return {};
    }
    return {};
}

// KWin's geometry is fractional under display scaling, so both scripts
// round. Transients are DeFragR's dialogs, never the main window.
QString Placement::readFrameScript(qint64 pid, const QString &marker)
{
    return QStringLiteral(R"(var wins = workspace.windowList();
for (var i = 0; i < wins.length; i++) {
    var c = wins[i];
    if (c.pid !== %1 || c.transientFor) continue;
    print("%2", Math.round(c.frameGeometry.x), Math.round(c.frameGeometry.y));
    break;
}
)")
        .arg(pid)
        .arg(marker);
}

// Kept on the screen that holds `at`, or on the window's own screen when
// that one is gone. KWin.PlacementArea: workspace.PlacementArea is
// undefined on Plasma 6.
QString Placement::moveFrameScript(qint64 pid, QPoint at)
{
    return QStringLiteral(R"(var x = %2;
var y = %3;
var wins = workspace.windowList();
for (var i = 0; i < wins.length; i++) {
    var c = wins[i];
    if (c.pid !== %1 || c.transientFor) continue;
    var g = c.frameGeometry, area = null;
    for (var j = 0; j < workspace.screens.length; j++) {
        var s = workspace.screens[j].geometry;
        if (x >= s.x && x < s.x + s.width && y >= s.y && y < s.y + s.height)
            area = workspace.clientArea(KWin.PlacementArea, workspace.screens[j],
                                        workspace.currentDesktop);
    }
    if (area === null) area = workspace.clientArea(KWin.PlacementArea, c);
    x = Math.round(Math.max(area.x, Math.min(x, area.x + area.width - g.width)));
    y = Math.round(Math.max(area.y, Math.min(y, area.y + area.height - g.height)));
    c.frameGeometry = { x: x, y: y, width: g.width, height: g.height };
    break;
}
)")
        .arg(pid)
        .arg(at.x())
        .arg(at.y());
}

void Placement::restore()
{
    QScreen *screen = m_window.screen();
    const QRect room = screen ? screen->availableGeometry() : QRect();
    // Nothing saved, or nothing usable, keeps the start size. A size saved
    // on a bigger screen shrinks to this one's.
    const QSize saved = m_settings->value(kSizeKey).toSize();
    if (!saved.isEmpty())
        m_window.resize(room.isValid() ? saved.boundedTo(room.size()) : saved);

    const QVariant position = m_settings->value(kPositionKey);
    if (position.typeId() != QMetaType::QPoint)
        return;
    m_savedPosition = position.toPoint();
    if (onWayland())
        return;  // moved by KWin once shown
    QScreen *holder = QGuiApplication::screenAt(*m_savedPosition);
    if (!holder)
        holder = screen;
    m_window.move(holder ? keptWithin(*m_savedPosition, m_window.size(), holder->availableGeometry())
                         : *m_savedPosition);
}

void Placement::save()
{
    // The size it would have unmaximised, so a maximised window comes back
    // as the window it was before.
    m_settings->setValue(kSizeKey, m_window.normalGeometry().size());
    const std::optional<QPoint> frame = onWayland() ? frameFromKWin() : m_window.pos();
    // Unknown is left as it was: the last place known beats none.
    if (frame)
        m_settings->setValue(kPositionKey, *frame);
    m_settings->sync();
}

void Placement::moveWhenExposed()
{
    QWindow *handle = m_window.windowHandle();
    if (!handle)
        return;
    if (handle->isExposed()) {
        // Once only: shown again later, the window stays where the user
        // has it.
        QTimer::singleShot(kMoveAfterExposeMs, this, [this] {
            const std::optional<QPoint> at = std::exchange(m_savedPosition, std::nullopt);
            if (!at || !m_window.isVisible())
                return;
            m_moveScript = std::make_unique<KWinScript>(
                moveFrameScript(QCoreApplication::applicationPid(), *at));
            QTimer::singleShot(kKeepMoveScriptMs, this, [this] { m_moveScript.reset(); });
        });
        return;
    }
    handle->installEventFilter(this);
}

bool Placement::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == &m_window) {
        if (event->type() == QEvent::Close)
            save();
        else if (event->type() == QEvent::Show && onWayland() && m_savedPosition)
            moveWhenExposed();
    } else if (event->type() == QEvent::Expose) {
        auto *handle = static_cast<QWindow *>(watched);
        if (handle->isExposed()) {
            handle->removeEventFilter(this);
            moveWhenExposed();
        }
    }
    return false;
}

}  // namespace defragr::ui
