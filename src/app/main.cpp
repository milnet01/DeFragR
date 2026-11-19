// Start-up: the application, its language, its theme, its icon, and the
// main window where the user left it.
#include "engine/RunLog.h"
#include "ui/Languages.h"
#include "ui/MainWindow.h"
#include "ui/Placement.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>
#include <QSysInfo>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DeFragR"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/defragr.png")));
    // Never named again, but it holds the translators it installs, so it
    // must outlive the window.
    const defragr::ui::Languages languages;

    // Desktop until the menu offers a choice (DFR-0021).
    const defragr::ui::Theme theme(defragr::ui::Theme::Id::Desktop, QApplication::palette(),
                                   QApplication::font());
    theme.apply();

    defragr::ui::MainWindow window(theme);
#ifdef DEFRAGR_LOG
    // The developers' builds only (DFR-0111). Without the file the
    // window runs as it would have.
    if (defragr::engine::startRunLog(defragr::engine::runLogFolder()))
        qCInfo(defragr::engine::runLog).noquote()
            << "window starts:" << window.windowTitle() << "| Qt" << qVersion() << "on"
            << QApplication::platformName() << "|" << QSysInfo::prettyProductName();
#endif
    // Here, not in the window, so the window's tests never read or write
    // the user's settings.
    new defragr::ui::Placement(window);
    window.show();

    return QApplication::exec();
}
