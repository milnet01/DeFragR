// Start-up: the application, its language, its icon, and the main window.
#include "ui/Languages.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DeFragR"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/defragr.png")));
    const defragr::ui::Languages languages;

    defragr::ui::MainWindow window;
    window.show();

    return QApplication::exec();
}
