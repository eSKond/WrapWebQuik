#include "wwqmainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    WWQMainWindow w;
    w.show();
    return a.exec();
}
