// hlds-convert-gui: the converter with a window. See MainWindow.hpp.

#include <QApplication>

#include "MainWindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Where QSettings keeps the two remembered folders.
    QApplication::setOrganizationName(QStringLiteral("hlds"));
    QApplication::setApplicationName(QStringLiteral("hlds-convert"));

    MainWindow window;
    window.show();
    return app.exec();
}
