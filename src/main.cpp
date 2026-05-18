#include <QApplication>
#include <QSurfaceFormat>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    // Request Core Profile before creating the app — required on macOS for GLSL 150+
    QSurfaceFormat fmt;
    fmt.setVersion(3, 2);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("OpenMPT");
    app.setOrganizationName("OpenMPT");

    MainWindow win;
    win.show();

    if (argc > 1)
        win.openFile(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}
