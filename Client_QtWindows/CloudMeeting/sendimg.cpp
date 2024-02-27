#include "sendimg.h"
#include "netheader.h"
#include <QDebug>
#include <cstring>
#include <QBuffer>

extern QUEUE_DATA<MESG> queue_send;  //消息发送队列

SendImg::SendImg(QObject *par):QThread(par)
{

}

//消费线程
void SendImg::run()
{
    WRITE_LOG("start sending picture thread: 0x%p", QThread::currentThreadId());
    m_isCanRun = true;
    for(;;)
    {
        queue_lock.lock(); //加锁

        while(imgqueue.size() == 0)  //如果发送队列为空，也就是没有需要发的视频帧(没开摄像头)
        {
            //qDebug() << this << QThread::currentThreadId();
            bool f = queue_waitCond.wait(&queue_lock, WAITSECONDS * 1000); //线程阻塞到这里2秒，两秒后还没有反应，说明莫得开摄像头，发送帧数据的线程可以退出了
			if (f == false) //timeout
			{
				QMutexLocker locker(&m_lock);
                if (m_isCanRun == false) //状态为不可运行
				{
                    queue_lock.unlock();
					WRITE_LOG("stop sending picture thread: 0x%p", QThread::currentThreadId());
                    return; //从这里的return就退出线程了
				}
			}
        }
        //往下就是有需要发送的视频帧的情况...
        QByteArray img = imgqueue.front();  //取出，保存
//        qDebug() << "取出队列:" << QThread::currentThreadId();
        imgqueue.pop_front(); //弹出
        queue_lock.unlock();//解锁
        queue_waitCond.wakeOne(); //唤醒添加线程,就是下面那个


        //构造消息体
        MESG* imgsend = (MESG*)malloc(sizeof(MESG));
        if (imgsend == NULL)
        {
            WRITE_LOG("malloc error");
            qDebug() << "malloc imgsend fail";
        }
        else
        {
            memset(imgsend, 0, sizeof(MESG)); //使用前先置零
            imgsend->msg_type = IMG_SEND; //设置消息类型
            imgsend->len = img.size(); //设置消息大小
            qDebug() << "img size :" << img.size();
            imgsend->data = (uchar*)malloc(imgsend->len); //imgsend消息体的data部分就是将要发送的数据本体，先分配空间
            if (imgsend->data == nullptr)
            {
                free(imgsend); //分配失败的情况，写日志，然后循环等待
				WRITE_LOG("malloc error");
				qDebug() << "send img error";
                continue;
            }
            else
            { //分配内存成功，将img数据copy到imgsend消息体中去
                memset(imgsend->data, 0, imgsend->len);
				memcpy_s(imgsend->data, imgsend->len, img.data(), img.size());
				//加入发送队列
                queue_send.push_msg(imgsend);  //它的工作只到将视频帧数据加入发送队列，具体发送交给发送线程去做
            }
        }
    }
}

//添加到队列线程
void SendImg::pushToQueue(QImage img)
{
    //压缩
    QByteArray byte;
    QBuffer buf(&byte);
    buf.open(QIODevice::WriteOnly); //只写方式打开缓冲区
    img.save(&buf, "JPEG");   //将图片保存为jpeg格式
    QByteArray ss = qCompress(byte);
    QByteArray vv = ss.toBase64(); //一些数据格式
//    qDebug() << "加入队列:" << QThread::currentThreadId();
    queue_lock.lock(); //队列锁上锁
    while(imgqueue.size() > QUEUE_MAXSIZE)
    {
        queue_waitCond.wait(&queue_lock); //如果队列长度超过了最大长度，就阻塞等待队列中有消息发出
    }
    imgqueue.push_back(vv); //将压缩后的数据插入队列
    queue_lock.unlock(); //队列锁解锁，插入完队列之后就可以解锁，上锁是为了保证顺序不会乱
    queue_waitCond.wakeOne(); //唤醒消费线程（就是上面那个）
}

void SendImg::ImageCapture(QImage img)
{
    pushToQueue(img);  //获取截图的功能就是把图片数据加入到队列中
}

void SendImg::clearImgQueue()
{
    qDebug() << "清空视频队列";
    QMutexLocker locker(&queue_lock);
    imgqueue.clear();
}


void SendImg::stopImmediately()
{
    QMutexLocker locker(&m_lock);
    m_isCanRun = false;
}
