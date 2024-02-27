#ifndef NETHEADER_H
#define NETHEADER_H

#include <QMetaType>    //管理元对象系统中的命名类型，可以简单理解为，让自定义类型也成为一个元对象类型(比如QString)可以在运行时创建和销毁
#include <QMutex>
#include <QQueue>
#include <QImage>
#include <QWaitCondition>

#define QUEUE_MAXSIZE 1500      //队列最大大小

#ifndef MB
#define MB 1024*1024
#endif

#ifndef KB
#define KB 1024
#endif

#ifndef WAITSECONDS
#define WAITSECONDS 2           //无响应等待时间（单位s）
#endif

#ifndef OPENVIDEO
#define OPENVIDEO "打开视频"
#endif

#ifndef CLOSEVIDEO
#define CLOSEVIDEO "关闭视频"
#endif

#ifndef OPENAUDIO
#define OPENAUDIO "打开音频"
#endif

#ifndef CLOSEAUDIO
#define CLOSEAUDIO "关闭音频"
#endif


#ifndef MSG_HEADER
#define MSG_HEADER 11           //定义消息头的大小为11字节 $开始#结束
#endif

enum MSG_TYPE
{
    IMG_SEND = 0,       //发送图片
    IMG_RECV,           //接收图片
    AUDIO_SEND,         //发送音频
    AUDIO_RECV,         //接收音频
    TEXT_SEND,          //发送文本
    TEXT_RECV,          //接收文本
    CREATE_MEETING,     //创建会议室
    EXIT_MEETING,       //退出会议
    JOIN_MEETING,       //加入会议
    CLOSE_CAMERA,       //关闭摄像头

    CREATE_MEETING_RESPONSE = 20,   //创建会议的回复
    PARTNER_EXIT = 21,              //与会人退出
    PARTNER_JOIN = 22,              //与会人加入
    JOIN_MEETING_RESPONSE = 23,     //加入会议的回复
    PARTNER_JOIN2 = 24,             //与会人加入的广播消息
    RemoteHostClosedError = 40,     //远程连接错误
    OtherNetError = 41              //其他网络错误
};
Q_DECLARE_METATYPE(MSG_TYPE);  //将MSG_TYPE定义为元对象类型

struct MESG //消息结构体
{
    MSG_TYPE msg_type;  //消息类型
    uchar* data;        //消息本体
    long len;           //消息长度
    quint32 ip;         //发送方ip
};
Q_DECLARE_METATYPE(MESG *);  //将MESG的指针定义为元对象类型

//-------------------------------

template<class T>
struct QUEUE_DATA //消息队列
{
private:
    QMutex send_queueLock;          //发送队列互斥锁
    QWaitCondition send_queueCond;  //发送队列条件变量
    QQueue<T*> send_queue;          //发送消息的队列本体
public:
    void push_msg(T* msg) //向队列中插入消息
    {
        send_queueLock.lock(); //上锁
        while(send_queue.size() > QUEUE_MAXSIZE)  //如果队列长度大于最大长，等待到有消息发出长度减小为止
        {
            send_queueCond.wait(&send_queueLock); //用信号量实现阻塞
        }
        send_queue.push_back(msg); //将消息msg插入到queue队列中
        send_queueLock.unlock(); //互斥锁解锁
        send_queueCond.wakeOne(); //唤醒一个阻塞的线程
    }

    T* pop_msg() //从队列中拿出消息
    {
        send_queueLock.lock();  //上锁
        while(send_queue.size() == 0)  //如果当前队列为空，无消息可拿
        {
            bool f = send_queueCond.wait(&send_queueLock, WAITSECONDS * 1000); //线程锁在这里，也就是阻塞在这里2s(*1000是因为参数默认ms)
            if(f == false)
            {
                send_queueLock.unlock(); //等待无果，解锁之后直接退出，走前一定要解锁
                return NULL;
            }
        }
        T* send = send_queue.front(); //从队列中弹出一条消息
        send_queue.pop_front(); //弹出刚才已经保存过的消息
        send_queueLock.unlock(); //解锁
        send_queueCond.wakeOne(); //唤醒一个阻塞的线程
        return send; //将拿到的消息返回
    }

    void clear() //清空队列
    {
        send_queueLock.lock(); //清空队列的操作也要上锁，避免大家都想清空一块挤进来的情况
        send_queue.clear(); //调用queue的clear()函数即可
        send_queueLock.unlock(); //解锁
    }
};

struct Log  //日志类，就是一条日志信息
{
    char *ptr; //信息本体
    uint len; //长度
};



void log_print(const char *, const char *, int, const char *, ...);  //用宏的方式定义一个写日志的动作
#define WRITE_LOG(LOGTEXT, ...) do{ \
    log_print(__FILE__, __FUNCTION__, __LINE__, LOGTEXT, ##__VA_ARGS__);\
}while(0);



#endif // NETHEADER_H
