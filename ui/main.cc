#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("rime-webdav 配置工具");
    app.setApplicationVersion("1.0");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}