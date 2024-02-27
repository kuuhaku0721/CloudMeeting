#ifndef LOGQUEUE_H
#define LOGQUEUE_H

#include <QThread>      //线程依赖
#include <QMutex>       //互斥锁
#include <queue>        //队列，用到它的先进先出特性
#include "netheader.h"  //网络使用的头文件依赖

class LogQueue : public QThread  //日志队列，写日志的线程，在Qt里面如果要使用多线程需要让一个类去继承线程类
{
private:
    void run();  //重现run函数，也就是线程主函数
    QMutex m_lock; //互斥锁
    bool m_isCanRun; //当前状态是否可运行，也就是判断线程是否被阻塞

    QUEUE_DATA<Log> log_queue; //日志队列，这里的QUEUE_DATA用到了模板类，有更好的复用效果
    FILE *logfile; //一个文件对象，用于保存日志信息
public:
    explicit LogQueue(QObject *parent = nullptr);
    void stopImmediately(); //立即停止
    void pushLog(Log*); //将日志信息插入队列
};

#endif // LOGQUEUE_H
