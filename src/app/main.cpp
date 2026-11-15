// Start-up. DFR-0005 replaces the bare window with ui/MainWindow.
#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DeFragR"));

    QWidget window;
    window.setWindowTitle(QStringLiteral("DeFragR"));
    window.show();

    return QApplication::exec();
}
