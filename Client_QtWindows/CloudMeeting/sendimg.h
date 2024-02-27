#ifndef SENDIMG_H
#define SENDIMG_H

#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QImage>
#include <QVideoFrame>
#include <QWaitCondition>


class SendImg : public QThread  //发送视频帧也是由一个线程来做，不影响主程序运行的情况下，发送线程一直工作
{
    Q_OBJECT
private:
    QQueue<QByteArray> imgqueue;  //视频帧，也就是图片，是二进制数据，所以要用QByteArray来存储，也是队列传输，利用它先进先出的特性
    QMutex queue_lock; //队列互斥锁
    QWaitCondition queue_waitCond; //条件变量
    void run() override; //线程主函数，重写的父类的run函数,负责执行线程行为
    QMutex m_lock; //线程互斥锁
    volatile bool m_isCanRun; //是否阻塞的标志状态
public:
    SendImg(QObject *par = NULL);

    void pushToQueue(QImage); //将视频帧加入到队列中，将由槽函数调用
public slots:
    void ImageCapture(QImage); //捕获到视频帧
    void clearImgQueue(); //线程结束时，清空视频帧队列
    void stopImmediately();
};

#endif // SENDIMG_H
