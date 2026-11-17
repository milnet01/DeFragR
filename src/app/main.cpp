// Start-up: the application, its language, its theme, its icon, and the
// main window.
#include "ui/Languages.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DeFragR"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/defragr.png")));
    const defragr::ui::Languages languages;

    // Desktop until the menu offers a choice (DFR-0021).
    const defragr::ui::Theme theme(defragr::ui::Theme::Id::Desktop, QApplication::palette(),
                                   QApplication::font());
    theme.apply();

    defragr::ui::MainWindow window(theme);
    window.show();

    return QApplication::exec();
}
