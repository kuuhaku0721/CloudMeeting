#include "netheader.h"
#include "logqueue.h"
#include <QDebug>
#include <time.h>

QUEUE_DATA<MESG> queue_send; //发送消息队列
QUEUE_DATA<MESG> queue_recv; //接收消息队列
QUEUE_DATA<MESG> audio_recv; //接收到的音频

LogQueue *logqueue = nullptr;  //实例化一个日志队列对象
//打印日志的行为函数
void log_print(const char *filename, const char *funcname, int line, const char *fmt, ...)
{
    Log *log = (Log *) malloc(sizeof (Log));  //实例化一个日志对象，它用来保存一条日志信息
    if(log == nullptr)
    {
        qDebug() << "malloc log fail";
    }
    else
    {
        memset(log, 0, sizeof (Log)); //内存空间置零，习惯性操作
        log->ptr = (char *) malloc(1 * KB); //log->ptr成员是保存日志信息的位置，首先分配了1KB的空间，如果只是文本内容的话，1KB不少了
        if(log->ptr == nullptr)
        {
            free(log); //分配空间失败，也即是内存中连1KB都没了，释放掉指针就可以返回错误信息了,没救了
            qDebug() << "malloc log.ptr fail";
            return;
        }
        else
        {
            memset(log->ptr, 0, 1 * KB); //分配空间成功之后要先置零再使用
            time_t t = time(NULL); //声明一个时间对象，用来标志日志的时间的
            int pos = 0; //控制写入时的偏移量
            int m = strftime(log->ptr + pos, KB - 2 - pos, "%F %H:%M:%S ", localtime(&t)); //通过函数localtime将当前时间写入
            pos += m; //偏移量往后挪

            m = snprintf(log->ptr + pos, KB - 2 - pos, "%s:%s::%d>>>", filename, funcname, line); //日志惯用三部分：文件名、函数、行
            pos += m; //偏移量往后挪

            va_list ap; //看参数列表，用到了不定参数，所以要用va_list配合va_start和va_end来使用不定参数
            va_start(ap, fmt); //参数里面填list对象和第一个参数，剩下的它会自己跟上
            m = _vsnprintf(log->ptr + pos, KB - 2 - pos, fmt, ap); //将后面的信息跟着写进去日志空间
			pos += m;
            va_end(ap); //全部写完，结束list
            strcat_s(log->ptr+pos, KB-pos, "\n");  //在最后的最后，当条日志信息写完之后拼一个换行符进去
            log->len = strlen(log->ptr); //设置好日志信息的长度
            logqueue->pushLog(log); //然后插入的日志队列中
            //向缓冲区(日志队列)写日志信息和将日志信息写入到文件中是分开的，这样可以保证在多用户在操作时不会因为文件操作耗时过长
            //向文件中写日志内容交给一个单独的线程来做，对于用户来说是效率高的
        }
    }
}
