#include "recvsolve.h"
#include <QMetaType>
#include <QDebug>
#include <QMutexLocker>
extern QUEUE_DATA<MESG> queue_recv;  //接收队列

void RecvSolve::stopImmediately()
{
    QMutexLocker locker(&m_lock);
    m_isCanRun = false;
}

RecvSolve::RecvSolve(QObject *par):QThread(par)
{
    qRegisterMetaType<MESG *>();
    m_isCanRun = true;
}
//从接收队列拿取数据
void RecvSolve::run()
{
    WRITE_LOG("start solving data thread: 0x%p", QThread::currentThreadId());
    for(;;) //无限循环，只要线程启动之后，就一直在这个循环里绕，只要接收队列里有数据就拿出来然后交给信号槽函数去处理
    {
        {
            QMutexLocker locker(&m_lock);
            if (m_isCanRun == false)
            {
                WRITE_LOG("stop solving data thread: 0x%p", QThread::currentThreadId());
                return;
            }
        }
        MESG * msg = queue_recv.pop_msg();  //从队列中拿出一条数据出来
        if(msg == NULL) continue; //如果消息是空就空转等待
		/*else free(msg);
		qDebug() << "取出队列:" << msg->msg_type;*/
        emit datarecv(msg); //拿到数据就发射信号，然后让槽函数去处理，这个槽函数在Widget里面
    }
}
