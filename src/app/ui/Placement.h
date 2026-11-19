// Placement: the main window opens where the user left it, at the size
// they left it (DFR-0109). Where the window system lets the program move
// its own window (X11, and offscreen in tests), Qt does it. Under Wayland
// only the compositor may: on KDE Plasma, KWin is asked through a one-shot
// script, and tells where the window is by printing to the user journal.
// Elsewhere only the
// size is kept. Nothing here fails or shows an error: the worst case is a
// window where the desktop puts it.
#pragma once

#include <QObject>
#include <QPoint>
#include <QSettings>
#include <QSize>

#include <memory>
#include <optional>

class QWidget;

namespace defragr::ui {

class KWinScript;

class Placement : public QObject {
    Q_OBJECT

public:
    // Gives `window` its saved size now, and its saved position now or,
    // under Wayland, once it is on screen. Saves both as it closes.
    // `settings` holds them; left out, the user's own settings. Make one
    // after the window has its start size, and before showing it. The
    // window owns it.
    explicit Placement(QWidget &window, std::unique_ptr<QSettings> settings = {});
    ~Placement() override;

    // The user's own settings: $XDG_CONFIG_HOME/DeFragR/DeFragR.conf,
    // which is usually ~/.config/DeFragR/DeFragR.conf.
    static std::unique_ptr<QSettings> userSettings();

    // The x and y KWin printed after `marker`, if a line of `journal` has
    // them.
    static std::optional<QPoint> frameFromJournal(const QString &journal, const QString &marker);
    // KWin scripts for process `pid`'s main window: print its frame's x
    // and y after `marker`, or move its frame to `at`, kept on a screen.
    static QString readFrameScript(qint64 pid, const QString &marker);
    static QString moveFrameScript(qint64 pid, QPoint at);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void restore();
    void save();
    // Under Wayland: waits for the window to be on screen, then asks KWin.
    void moveWhenExposed();

    QWidget &m_window;
    std::unique_ptr<QSettings> m_settings;
    std::optional<QPoint> m_savedPosition;
    // The script that moved the window, kept until KWin has run it.
    std::unique_ptr<KWinScript> m_moveScript;
};

}  // namespace defragr::ui
