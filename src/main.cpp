#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("HabitTrackerSDD");
    QApplication::setOrganizationName("HabitTrackerSDD");
    MainWindow window;
    window.show();
    return app.exec();
}
