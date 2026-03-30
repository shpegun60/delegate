#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    const bool autorunExit = QCoreApplication::arguments().contains(QStringLiteral("--autorun-exit"));
    a.setQuitOnLastWindowClosed(!autorunExit);

    MainWindow w;

    if (autorunExit) {
        QObject::connect(&w, &MainWindow::suiteFinished, &a, [&a](bool ok) {
            a.exit(ok ? 0 : 1);
        });
    } else {
        w.show();
    }

    return a.exec();
}
