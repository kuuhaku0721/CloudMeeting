#include "logqueue.h"
#include <QDebug>

LogQueue::LogQueue(QObject *parent) : QThread(parent) //默认构造、无特殊动作
{

}

void LogQueue::pushLog(Log* log)
{
    log_queue.push_msg(log); //向队列中插入一条日志数据
}
//重写的线程run函数，也就是线程主函数，负责将日志队列中的信息写入到文件中去
void LogQueue::run()
{
    m_isCanRun = true; //在函数一进来的时候线程情况一定是可用的
    for(;;) //一个无限循环，持续的将队列中的消息写入到文件中，只要在别的线程插入了消息，在这里就能把消息写进去文件
    {
        { //这一部分是个代码块
            QMutexLocker lock(&m_lock); //在进行一切操作之前先上锁
            if(m_isCanRun == false) //如果线程已经阻塞掉了，变成了不可用状态，这里就可以直接返回了，走之前记得关闭文件。这是在循环里面，会出现这种情况
            {
                fclose(logfile);
                return;
            }
        }
        Log *log = log_queue.pop_msg(); //Log类是一个单条日志对象类，作用是从日志队列里拿到一条消息出来
        if(log == NULL || log->ptr == NULL) continue; //队列为空的情况，也就是队列中暂时没有信息可写，那就进入循环等待状态，一直循环空转到队列中有消息

        //----------------write to logfile-------------------
        errno_t r = fopen_s(&logfile, "./log.txt", "a"); //errno_t 就是错误代码errno，在Qt中也是一个单独的元对象，可以保留返回值错误类型
        if(r != 0)
        {
            qDebug() << "打开文件失败:" << r;
            continue; //打开失败也循环等待，直到打开成功才能开始写入逻辑
        }


        qint64 hastowrite = log->len; //保存一下需要写入的长度，当需要写入的数据过大时需要用这种方法来分多次写入
        qint64 ret = 0, haswrite = 0; //ret返回值判断当次写入状态，haswrite是已经写入的长度
        while ((ret = fwrite( (char*)log->ptr + haswrite, 1 ,hastowrite - haswrite, logfile)) < hastowrite) //以循环的形式通过fwrite不断地向文件中写入数据
        {
            if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) ) //就是EINTER一样属于正常中断信息，不用特殊处理继续循环就行
            {
                ret = 0;
            }
            else
            { //但如果不是上面两种情况地话问题就大了，这一整条数据都没法正常写入，所以直接退出循环
                qDebug() << "write logfile error";
                break;
            }
            haswrite += ret; //ret返回的是fwrite写入成功地长度，已经写的部分和需要写的都加上偏移量，用于控制写入坐标
            hastowrite -= ret;
        }

        //free
        if(log->ptr) free(log->ptr); //当条信息写入完毕，释放掉log对象
        if(log) free(log);

        fflush(logfile); //刷新文件缓冲区，从缓存中写入到磁盘文件
        fclose(logfile); //关闭文件
    }
}

void LogQueue::stopImmediately() //虽然函数名叫立即停止，但实际上不能立马关停系统，所以需要先让它阻塞掉，然后让它不可用，关闭文件，清理残留资源，然后才算停止
{
    QMutexLocker lock(&m_lock);
    m_isCanRun = false;
}
