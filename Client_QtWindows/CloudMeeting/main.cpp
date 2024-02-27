#include <QApplication>
#include "widget.h"
#include "screen.h"
#include <QTextCodec>
int main(int argc, char* argv[])
{
    //设置编码，避免因不同平台导致中文乱码问题
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF8"));
    QApplication app(argc, argv);
    Screen::init(); //其他都是默认的，只有这个是自己的，作用是：初始化屏幕，获取当前主屏幕的宽和高

    Widget w;
    w.show();
    return app.exec();
}
