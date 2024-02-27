#ifndef SENDTEXT_H
#define SENDTEXT_H

#include <QThread>
#include <QMutex>
#include <QQueue>
#include "netheader.h"

struct M //其实还是就是一个msg消息结构体，只不过封装的东西少一点，只有一个字符串和一个类型
{
    QString str;
    MSG_TYPE type;

    M(QString s, MSG_TYPE e)
    {
        str = s;
        type = e;
    }
};

//发送文本数据
class SendText : public QThread  //发送文本数据也是单独的一个线程处理
{
    Q_OBJECT
private:
    QQueue<M> textqueue; //发送文本队列
    QMutex textqueue_lock; //队列锁
    QWaitCondition queue_waitCond; //条件变量
    void run() override; //线程主函数
    QMutex m_lock; //互斥锁
    bool m_isCanRun;
public:
    SendText(QObject *par = NULL);
    ~SendText();
public slots:
    void push_Text(MSG_TYPE, QString str = ""); //将文本内容插入队列，有个默认值是空串
    void stopImmediately();
};

#endif // SENDTEXT_H
