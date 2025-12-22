#include <QApplication>
#include <QFile>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // 解决高分屏缩放乱码或模糊问题
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);

    // 加载 QSS 皮肤
    QFile file("style.qss");
    if (file.open(QFile::ReadOnly)) {
        a.setStyleSheet(file.readAll());
    }

    MainWindow w;
    w.show();
    return a.exec();
}