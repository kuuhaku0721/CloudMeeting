#include "sendtext.h"
#include <QDebug>

extern QUEUE_DATA<MESG> queue_send;    //引入一个发送队列
#ifndef WAITSECONDS
#define WAITSECONDS 2
#endif
SendText::SendText(QObject *par):QThread(par)
{

}

SendText::~SendText()
{

}
//将文本内容插入到发送队列中去
void SendText::push_Text(MSG_TYPE msgType, QString str)
{
    textqueue_lock.lock();  //进入先上锁
    while(textqueue.size() > QUEUE_MAXSIZE)
    {
        queue_waitCond.wait(&textqueue_lock);
    }
    textqueue.push_back(M(str, msgType));  //把文本字符串和类型封装一下加入到队列中

    textqueue_lock.unlock(); //离开解锁
    queue_waitCond.wakeOne(); //顺便唤醒发送线程
}


void SendText::run()
{
    m_isCanRun = true;
    WRITE_LOG("start sending text thread: 0x%p", QThread::currentThreadId());
    for(;;) //一个无限循环，只要有文本消息进入队列就把它发送出去
    {
        textqueue_lock.lock(); //加锁
        while(textqueue.size() == 0) //如果没有需要发送的文本内容，等待2s无响应的话就退出线程，不然线程空转
        {
            bool f = queue_waitCond.wait(&textqueue_lock, WAITSECONDS * 1000);
            if(f == false) //timeout
            {
                QMutexLocker locker(&m_lock);
                if(m_isCanRun == false)
                {
                    textqueue_lock.unlock();
					WRITE_LOG("stop sending text thread: 0x%p", QThread::currentThreadId());
                    return;
                }
            }
        }

        M text = textqueue.front(); //从队列中取出来一条信息

//        qDebug() << "取出队列:" << QThread::currentThreadId();

        textqueue.pop_front(); //保存完就弹出
        textqueue_lock.unlock();//解锁
        queue_waitCond.wakeOne(); //唤醒添加线程

        //构造消息体
        MESG* send = (MESG*)malloc(sizeof(MESG));
        if (send == NULL)
        {
            WRITE_LOG("malloc error");
            qDebug() << __FILE__  <<__LINE__ << "malloc fail";
            continue; //分配内存出错不需要退出，只需要进入循环等待即可，说不定下次来到这儿的时候就有内存了呢
        }
        else
        {
            memset(send, 0, sizeof(MESG)); //使用前置零
            //不同类型的消息携带的内容不一样，要分开单独处理
			if (text.type == CREATE_MEETING || text.type == CLOSE_CAMERA)
			{
                send->len = 0; //创建会议和关闭摄像头没有消息体只有控制指令
				send->data = NULL;
                send->msg_type = text.type; //所以只需要写个类型进去就够了
                queue_send.push_msg(send); //然后加入到发送队列就行了
			}
			else if (text.type == JOIN_MEETING)
            {   //加入会议有一个房间号
				send->msg_type = JOIN_MEETING;
				send->len = 4; //房间号占4个字节
                send->data = (uchar*)malloc(send->len + 10); //给房间号准备个内存空间
                
                if (send->data == NULL)
                {
                    WRITE_LOG("malloc error");
                    qDebug() << __FILE__ << __LINE__ << "malloc fail";
                    free(send);
                    continue;                                   //上下准备和拷贝，也就是说中间这么大一堆其实没啥用，都是差错控制，但一般不会出错
                }
                else
                {
                    memset(send->data, 0, send->len + 10);
					quint32 roomno = text.str.toUInt();
                    memcpy(send->data, &roomno, sizeof(roomno)); //然后把房间号保存下来
					//加入发送队列
					queue_send.push_msg(send);
                }
			}
            else if(text.type == TEXT_SEND)
            {   //发送文本的类型需要将文本内容压缩后发送
                send->msg_type = TEXT_SEND;
                QByteArray data = qCompress(QByteArray::fromStdString(text.str.toStdString())); //压缩
                send->len = data.size();
                send->data = (uchar *) malloc(send->len); //分配一下内存
                if(send->data == NULL)
                {
                    WRITE_LOG("malloc error");
                    qDebug() << __FILE__ << __LINE__ << "malloc error";
                    free(send);
                    continue;
                }
                else
                {
                    memset(send->data, 0, send->len);
                    memcpy_s(send->data, send->len, data.data(), data.size()); //把数据内容拷贝进去发送消息体
                    queue_send.push_msg(send); //加入发送队列
                }
            }
        }
    }
}
void SendText::stopImmediately()
{
    QMutexLocker locker(&m_lock);
    m_isCanRun = false;
}
