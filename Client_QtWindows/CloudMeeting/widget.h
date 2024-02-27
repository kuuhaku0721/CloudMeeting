#ifndef WIDGET_H
#define WIDGET_H

#include "AudioInput.h"     //音频的输入
#include <QWidget>
#include <QVideoFrame>      //视频框架依赖头文件
#include <QTcpSocket>       //TCP通信依赖头文件
#include "mytcpsocket.h"    //对tcp的封装
#include <QCamera>          //摄像头使用依赖头文件
#include "sendtext.h"       //发送消息模块
#include "recvsolve.h"      //接收消息处理模块
#include "partner.h"        //用户加入处理
#include "netheader.h"      //网络数据收发模块
#include <QMap>             //map..
#include "AudioOutput.h"    //音频输出
#include "chatmessage.h"    //会议聊天模块
#include <QStringListModel> //字符串列表模型

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

//先声明这些内容是:类，避免在使用的时候因为编译器识别问题导致出错
class QCameraImageCapture;      //摄像头截图实现类
class MyVideoSurface;           //视频窗口控制类
class SendImg;                  //发送截图(视频就是一帧又一帧的图片)类
class QListWidgetItem;          //用于显示用户列表的类


class Widget : public QWidget
{
    Q_OBJECT
private:
    static QRect pos;   //一个静态变量，用于保存当前光标位置
    quint32 mainip; //主屏幕显示的IP图像
    QCamera *_camera; //摄像头
    QCameraImageCapture *_imagecapture; //截屏
    bool  _createmeet; //是否创建会议
    bool _joinmeet; // 加入会议
    bool _openCamera; //是否开启摄像头

    MyVideoSurface *_myvideosurface;                //TODO:这里有一些暂时还不知道是什么的东西，等一个类一个类看过来的时候再去详细的了解

    QVideoFrame mainshow;

    SendImg *_sendImg;
    QThread *_imgThread;

    RecvSolve *_recvThread;
    SendText * _sendText;
    QThread *_textThread;
    //socket
    MyTcpSocket * _mytcpSocket;
    void paintEvent(QPaintEvent *event);

    QMap<quint32, Partner *> partner; //用于记录房间用户
    Partner* addPartner(quint32);
    void removePartner(quint32);
    void clearPartner(); //退出会议，或者会议结束
    void closeImg(quint32); //根据IP重置图像

    void dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type); //用户发送文本
    void dealMessageTime(QString curMsgTime); //处理时间

    //音频
    AudioInput* _ainput;
    QThread* _ainputThread;
    AudioOutput* _aoutput;

    QStringList iplist;

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_createmeetBtn_clicked();    //创建按钮
    void on_exitmeetBtn_clicked();      //退出按钮

    void on_openVedio_clicked();        //打开摄像头按钮
    void on_openAudio_clicked();        //打开音频按钮
    void on_connServer_clicked();       //连接按钮
    void cameraError(QCamera::Error);
    void audioError(QString);
//    void mytcperror(QAbstractSocket::SocketError);
    void datasolve(MESG *);
    void recvip(quint32);
    void cameraImageCapture(QVideoFrame frame);
    void on_joinmeetBtn_clicked();      //加入按钮

    void on_horizontalSlider_valueChanged(int value);   //音量条
    void speaks(QString);

    void on_sendmsg_clicked();          //发送按钮

    void textSend();

signals:
    void pushImg(QImage);
    void PushText(MSG_TYPE, QString = "");
    void stopAudio();
    void startAudio();
    void volumnChange(int);
private:
    Ui::Widget *ui;
};
#endif // WIDGET_H
