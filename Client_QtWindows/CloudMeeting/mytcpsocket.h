#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QThread>
#include <QTcpSocket>
#include <QMutex>
#include "netheader.h"
#ifndef MB
#define MB (1024 * 1024)
#endif

typedef unsigned char uchar;


class MyTcpSocket: public QThread   //套接字网络连接也是单独线程来处理
{
    Q_OBJECT
public:
    ~MyTcpSocket();
    MyTcpSocket(QObject *par=NULL);
    bool connectToServer(QString, QString, QIODevice::OpenModeFlag); //连接到服务器
    QString errorString(); //错误字符串
    void disconnectFromHost(); //从服务器断开连接
    quint32 getlocalip(); //获取本机ip
private:
    void run() override; //线程主函数
    qint64 readn(char *, quint64, int); //从套接口读取消息
    QTcpSocket *_socktcp; //套接字本体变量
    QThread *_sockThread; //套接字线程本体
    uchar *sendbuf; //发送缓冲区
    uchar* recvbuf; //接收缓冲区
    quint64 hasrecvive; //标记接收的偏移量

    QMutex m_lock; //互斥锁
    volatile bool m_isCanRun; //标记是否可运行状态
private slots:
    bool connectServer(QString, QString, QIODevice::OpenModeFlag); //连接服务器槽函数
    void sendData(MESG *); //发送槽函数
    void closeSocket(); //断开连接槽函数

public slots:
    void recvFromSocket(); //接收消息槽函数
    void stopImmediately();
    void errorDetect(QAbstractSocket::SocketError error);
signals:
    void socketerror(QAbstractSocket::SocketError);
    void sendTextOver();
};

#endif // MYTCPSOCKET_H
