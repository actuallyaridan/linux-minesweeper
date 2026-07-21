#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("minesweeper");
    app.setApplicationName("minesweeper");
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/Minesweeper.ico")));
    MainWindow w;
    w.show();
    return app.exec();
}
