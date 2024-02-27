#ifndef RECVSOLVE_H
#define RECVSOLVE_H
#include "netheader.h"
#include <QThread>
#include <QMutex>
/*
 * 接收线程
 * 从接收队列拿取数据
 */
class RecvSolve : public QThread
{
    Q_OBJECT
public:
    RecvSolve(QObject *par = NULL);
    void run() override;  //线程主函数
private:
    QMutex m_lock;
    bool m_isCanRun;
signals:
    void datarecv(MESG *);  //只有一个接收数据的信号
public slots:
    void stopImmediately();
};

#endif // RECVSOLVE_H
