#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QWidget>
#include <QLabel>

class ChatMessage:public QWidget   //聊天记录是显示在ListWidget上的一个个Widget控件，所以继承自Widget
{
    Q_OBJECT
public:
    explicit ChatMessage(QWidget *parent = nullptr);

    enum User_Type{
        User_System,//系统
        User_Me,    //自己
        User_She,   //用户
        User_Time,  //时间
    };
    void setTextSuccess();
    void setText(QString text, QString time, QSize allSize, QString ip = "",  User_Type userType = User_Time); //设置文本内容

    QSize getRealString(QString src); //仅仅只是文本的大小,不带外面全部的margin框
    QSize fontRect(QString str); //字体大小

    inline QString text() {return m_msg;}
    inline QString time() {return m_time;}
    inline User_Type userType() {return m_userType;}
protected:
    void paintEvent(QPaintEvent *event); //重写绘图事件，用于在ListWidget上每发出一条消息就显示出来一条
private:
    QString m_msg;      //都是用于显示的文本内容
    QString m_time;
    QString m_curTime;
    QString m_ip;

    QSize m_allSize;
    User_Type m_userType = User_System;

    int m_kuangWidth; //框的宽
    int m_textWidth; //文本的宽
    int m_spaceWid; //整个区域的宽
    int m_lineHeight; //线高

    QRect m_ipLeftRect; //显示的ip的左侧区域
    QRect m_ipRightRect;
    QRect m_iconLeftRect; //图标(头像)的左侧区域
    QRect m_iconRightRect;
    QRect m_sanjiaoLeftRect; //发送的三角左侧区域
    QRect m_sanjiaoRightRect;
    QRect m_kuangLeftRect; //整个包围框的左侧区域
    QRect m_kuangRightRect;
    QRect m_textLeftRect; //文本内容的左侧区域
    QRect m_textRightRect;
    QPixmap m_leftPixmap; //左侧图片
    QPixmap m_rightPixmap; //右侧图片
    QLabel* m_loading = Q_NULLPTR; //加载头像的
    QMovie* m_loadingMovie = Q_NULLPTR; //发送时 加载 的的动画效果
    bool m_isSending = false; //发送状态
};

#endif // CHATMESSAGE_H
