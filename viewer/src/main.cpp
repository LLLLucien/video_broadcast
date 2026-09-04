#include "viewer_window.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ViewerWindow window;
    window.show();
    return app.exec();
}
