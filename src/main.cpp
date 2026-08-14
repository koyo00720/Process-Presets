#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setWindowIcon(QIcon(QStringLiteral(":/src/icon.png")));
    QCoreApplication::setOrganizationName(QStringLiteral("ProcessPresets"));
    QCoreApplication::setApplicationName(QStringLiteral("ProcessPresets"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PROCESS_PRESETS_VERSION));

    ProcessPresets::MainWindow window;
    window.show();
    return application.exec();
}
