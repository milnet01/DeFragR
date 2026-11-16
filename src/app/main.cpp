// Start-up: the application, its icon, and the main window.
#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DeFragR"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/defragr.png")));

    defragr::ui::MainWindow window;
    window.show();

    return QApplication::exec();
}
