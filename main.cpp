#include "postboy_win.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    postboy_win w;
    w.show();
    return a.exec();
}
