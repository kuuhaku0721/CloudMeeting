#include "mytcpsocket.h"
#include "netheader.h"
#include <QHostAddress>
#include <QtEndian>
#include <QMetaObject>
#include <QMutexLocker>

extern QUEUE_DATA<MESG> queue_send;  //发送队列
extern QUEUE_DATA<MESG> queue_recv;  //接收队列
extern QUEUE_DATA<MESG> audio_recv;  //音频接收队列

void MyTcpSocket::stopImmediately() //关闭、退出
{
    {
        QMutexLocker lock(&m_lock);
        if(m_isCanRun == true) m_isCanRun = false;
    }
    //关闭read
    _sockThread->quit();
    _sockThread->wait();
}

void MyTcpSocket::closeSocket() //断开套接字连接
{
    if (_socktcp && _socktcp->isOpen()) //如果套接字还活着并且处于打开状态才能关
	{
		_socktcp->close();
	}
}

MyTcpSocket::MyTcpSocket(QObject *par):QThread(par)
{
    qRegisterMetaType<QAbstractSocket::SocketError>(); //把套接字错误SocketError也注册成一个元对象，万一运行过程中出错了也好找
    _socktcp = nullptr; //连接前先初始化为空

    _sockThread = new QThread(); //发送数据线程
    this->moveToThread(_sockThread); //将当前类的消息循环交给新线程sockThread去处理
	connect(_sockThread, SIGNAL(finished()), this, SLOT(closeSocket()));
    sendbuf =(uchar *) malloc(4 * MB); //分配给发送和接收的缓冲区都是4M大小
    recvbuf = (uchar*)malloc(4 * MB);
    hasrecvive = 0; //已接收到的大小为0 初始化一下

}

//错误追踪
void MyTcpSocket::errorDetect(QAbstractSocket::SocketError error)
{
    qDebug() <<"Sock error" <<QThread::currentThreadId();
    MESG * msg = (MESG *) malloc(sizeof (MESG)); //准备一个消息体，用来保存错误信息
    if (msg == NULL)
    {
        qDebug() << "errdect malloc error";
    }
    else
    {
        memset(msg, 0, sizeof(MESG)); //使用前置零
        if (error == QAbstractSocket::RemoteHostClosedError) //远程连接错误
		{
            msg->msg_type = RemoteHostClosedError; //设置一下消息类型
		}
		else
		{
			msg->msg_type = OtherNetError;
		}
        queue_recv.push_msg(msg); //接收消息队列中插入一条错误信息。因为如果发生了这个错误，需要处理的是客户端自己，而不是服务器
    }
}



void MyTcpSocket::sendData(MESG* send)  //发送数据  会被下面那个run函数的循环调用
{
	if (_socktcp->state() == QAbstractSocket::UnconnectedState)
    { //如果套接字处于未连接状态
        emit sendTextOver(); //发射信号通知上层发送线程挂了
        if (send->data) free(send->data); //释放掉这块数据占用的内存空间
        if (send) free(send);
        return; //退出
	}
    quint64 bytestowrite = 0;  //标记需要写入的偏移量
	//构造消息头
    sendbuf[bytestowrite++] = '$'; //消息格式控制 大小为11 以$开始以#结束

	//消息类型
    qToBigEndian<quint16>(send->msg_type, sendbuf + bytestowrite); //写入消息类型
    bytestowrite += 2; //修改偏移量

	//发送者ip
	quint32 ip = _socktcp->localAddress().toIPv4Address();
    qToBigEndian<quint32>(ip, sendbuf + bytestowrite); //写入发送方ip地址
    bytestowrite += 4; //修改偏移量

    if (send->msg_type == CREATE_MEETING || send->msg_type == AUDIO_SEND || send->msg_type == CLOSE_CAMERA || send->msg_type == IMG_SEND || send->msg_type == TEXT_SEND) //创建会议,发送音频,关闭摄像头，发送图片
	{
		//发送数据大小
		qToBigEndian<quint32>(send->len, sendbuf + bytestowrite);
		bytestowrite += 4;
	}
	else if (send->msg_type == JOIN_MEETING)
	{
        qToBigEndian<quint32>(send->len, sendbuf + bytestowrite); //发送数据大小
		bytestowrite += 4;
        uint32_t room; //如果是加入房间的话，需要把想要加入的房间的房间信息也发送出去让服务端处理
		memcpy(&room, send->data, send->len);
		qToBigEndian<quint32>(room, send->data);
	}

	//将数据拷入sendbuf
	memcpy(sendbuf + bytestowrite, send->data, send->len);
	bytestowrite += send->len;
	sendbuf[bytestowrite++] = '#'; //结尾字符

	//----------------write to server-------------------------
    qint64 hastowrite = bytestowrite; //转移一下偏移量，准备写入套接字发给服务端
	qint64 ret = 0, haswrite = 0;
	while ((ret = _socktcp->write((char*)sendbuf + haswrite, hastowrite - haswrite)) < hastowrite)
	{
		if (ret == -1 && _socktcp->error() == QAbstractSocket::TemporaryError)
		{
            ret = 0;
		}
		else if (ret == -1)
		{
			qDebug() << "network error";
			break;
		}
		haswrite += ret;
		hastowrite -= ret;
	}

    _socktcp->waitForBytesWritten(); //套接字自带一个等待字节流写入完毕的函数

    if(send->msg_type == TEXT_SEND)
    {
        emit sendTextOver(); //成功往内核发送文本信息
    }


    if (send->data) //发送完毕就释放内存空间，等待装下一条
	{
		free(send->data);
	}
	//free
	if (send)
	{
		free(send);
	}
}

/*
 * 发送线程
 */
void MyTcpSocket::run()
{
    //qDebug() << "send data" << QThread::currentThreadId();
    m_isCanRun = true; //标记可以运行
    /*
    *$_MSGType_IPV4_MSGSize_data_# //
    * 1 2 4 4 MSGSize 1
    *底层写数据线程
    */
    for(;;)  //一个无限循环 队列中有消息就发出去
    {
        {
            QMutexLocker locker(&m_lock);
            if(m_isCanRun == false) return; //在每次循环判断是否可以运行，如果不行就退出循环
        }
        
        //构造消息体
        MESG * send = queue_send.pop_msg();
        if(send == NULL) continue;
        QMetaObject::invokeMethod(this, "sendData", Q_ARG(MESG*, send)); //(类型，值)
        //Qt元对象的方法，调用方法 参数：调用者自己，方法名用字符串指定 参数用Q_ARG宏的方式指定
    }
}


qint64 MyTcpSocket::readn(char * buf, quint64 maxsize, int n)
{
    quint64 hastoread = n;  //从套接字中读取信息
    quint64 hasread = 0; //标记读取偏移量的
    do
    {
        qint64 ret  = _socktcp->read(buf + hasread, hastoread);
        if(ret < 0)
        {
            return -1;
        }
        if(ret == 0)
        {
            return hasread;
        }
        hasread += ret;
        hastoread -= ret;
    }while(hastoread > 0 && hasread < maxsize);
    return hasread;
}


void MyTcpSocket::recvFromSocket()
{

    //qDebug() << "recv data socket" <<QThread::currentThread();
    /*
    *  格式： $_msgtype_ip_size_data_#
    */
    qint64 availbytes = _socktcp->bytesAvailable(); //可获取的字节数,标记读取大小
	if (availbytes <=0 )
	{
		return;
	}
    qint64 ret = _socktcp->read((char *) recvbuf + hasrecvive, availbytes); //从套接字中读取消息到recvbuf缓冲区中，大小是availbytes个
    if (ret <= 0)
    {
        qDebug() << "error or no more data";
		return;
    }
    hasrecvive += ret; //更改偏移量

    //数据包不够
    if (hasrecvive < MSG_HEADER)  //数据包头大小不够，数据有错，直接返回
    {
        return;
    }
    else
    {
        quint32 data_size;  //数据包大小
        qFromBigEndian<quint32>(recvbuf + 7, 4, &data_size); //接收缓冲区加上偏移量，保存数据包大小
        if ((quint64)data_size + 1 + MSG_HEADER <= hasrecvive) //收够一个包
        {
            if (recvbuf[0] == '$' && recvbuf[MSG_HEADER + data_size] == '#') //且包结构正确
            {
                MSG_TYPE msgtype;  //消息类型
				uint16_t type;
                qFromBigEndian<uint16_t>(recvbuf + 1, 2, &type);  //从接收缓冲区中拿到类型出来
                msgtype = (MSG_TYPE)type; //强转一下类型
				qDebug() << "recv data type: " << msgtype;
				if (msgtype == CREATE_MEETING_RESPONSE || msgtype == JOIN_MEETING_RESPONSE || msgtype == PARTNER_JOIN2)
				{
                    if (msgtype == CREATE_MEETING_RESPONSE)  //如果收到的消息是对创建会议的回复
					{
                        qint32 roomNo; //创建会议时服务端会发回来一个房间号
                        qFromBigEndian<qint32>(recvbuf + MSG_HEADER, 4, &roomNo); //把房间号拿出来

                        MESG* msg = (MESG*)malloc(sizeof(MESG)); //一个消息体用于保存消息内容,这个消息是接收到的消息，准备交给其他线程去处理的

						if (msg == NULL)
						{
							qDebug() << __LINE__ << " CREATE_MEETING_RESPONSE malloc MESG failed";
						}
						else
						{
                            memset(msg, 0, sizeof(MESG)); //使用前置零
                            msg->msg_type = msgtype; //写入消息类型
                            msg->data = (uchar*)malloc((quint64)data_size); //分配一下内存空间，准备写入消息数据
							if (msg->data == NULL)
							{
								free(msg);
								qDebug() << __LINE__ << "CREATE_MEETING_RESPONSE malloc MESG.data failed";
							}
							else
							{
								memset(msg->data, 0, (quint64)data_size);
                                memcpy(msg->data, &roomNo, data_size); //把房间号内容写进去消息内容部分
								msg->len = data_size;
                                queue_recv.push_msg(msg); //插入到接收队列中，然后等待其他线程调用然后处理
							}

						}
					}
                    else if (msgtype == JOIN_MEETING_RESPONSE)  //如果是加入会议的回复
					{
						qint32 c;
                        memcpy(&c, recvbuf + MSG_HEADER, data_size);  //从接收的消息缓冲区中拿到控制置零

                        MESG* msg = (MESG*)malloc(sizeof(MESG)); //一个消息体，用于保存当次消息内容，等待处理线程根据消息类型进行处理

						if (msg == NULL)
						{
							qDebug() << __LINE__ << "JOIN_MEETING_RESPONSE malloc MESG failed";
						}
						else
						{
							memset(msg, 0, sizeof(MESG));
							msg->msg_type = msgtype;
							msg->data = (uchar*)malloc(data_size);
							if (msg->data == NULL)
							{
								free(msg);
								qDebug() << __LINE__ << "JOIN_MEETING_RESPONSE malloc MESG.data failed";
							}
							else
							{
								memset(msg->data, 0, data_size);
                                memcpy(msg->data, &c, data_size); //把消息内容拷贝进去之后

								msg->len = data_size;
                                queue_recv.push_msg(msg); //插入到接收队列中，等待处理即可
							}
						}
					}
                    else if (msgtype == PARTNER_JOIN2) //如果是有人加入的广播类型消息
					{
                        MESG* msg = (MESG*)malloc(sizeof(MESG)); //封装一下消息体，因为是广播消息，要发给所有人
						if (msg == NULL)
						{
							qDebug() << "PARTNER_JOIN2 malloc MESG error";
						}
						else
						{
                            memset(msg, 0, sizeof(MESG)); //置零，写入类型、长度、给数据部分分配内存
							msg->msg_type = msgtype;
							msg->len = data_size;
							msg->data = (uchar*)malloc(data_size);
							if (msg->data == NULL)
							{
								free(msg);
								qDebug() << "PARTNER_JOIN2 malloc MESG.data error";
							}
							else
							{
								memset(msg->data, 0, data_size);
								uint32_t ip;
								int pos = 0;
                                for (int i = 0; i < data_size / sizeof(uint32_t); i++)  //循环遍历，把当前活着的所有ip都封装到消息体里，然后交给处理线程
								{
									qFromBigEndian<uint32_t>(recvbuf + MSG_HEADER + pos, sizeof(uint32_t), &ip);
									memcpy_s(msg->data + pos, data_size - pos, &ip, sizeof(uint32_t));
									pos += sizeof(uint32_t);
								}
                                queue_recv.push_msg(msg); //插入到接收队列中，在这里只用把ip封装进去消息体不需要处理，具体的广播发送处理交给对应的线程解决
							}

						}
					}
				}
                else if (msgtype == IMG_RECV || msgtype == PARTNER_JOIN || msgtype == PARTNER_EXIT || msgtype == AUDIO_RECV || msgtype == CLOSE_CAMERA || msgtype == TEXT_RECV)
                { //这几个是带ip类型的消息，需要从收到的消息体中解析出来ip地址的
					//read ipv4
					quint32 ip;
                    qFromBigEndian<quint32>(recvbuf + 3, 4, &ip); //所以一进来首先就拿到ip地址出来

                    if (msgtype == IMG_RECV) //收到视频帧的处理
					{
						//QString ss = QString::fromLatin1((char *)recvbuf + MSG_HEADER, data_len);
						QByteArray cc((char *) recvbuf + MSG_HEADER, data_size);
						QByteArray rc = QByteArray::fromBase64(cc);
                        QByteArray rdc = qUncompress(rc);  //视频帧发送给服务端的时候用到了jpeg的格式压缩，收回来的时候要解压缩之后才能用
						//将消息加入到接收队列
		//                qDebug() << roomNo;
						
						if (rdc.size() > 0)
						{
                            MESG* msg = (MESG*)malloc(sizeof(MESG)); //将要用于封装的消息体
							if (msg == NULL)
							{
								qDebug() << __LINE__ << " malloc failed";
							}
							else
							{
                                memset(msg, 0, sizeof(MESG)); //使用前置零
								msg->msg_type = msgtype;
								msg->data = (uchar*)malloc(rdc.size()); // 10 = format + width + width
								if (msg->data == NULL)
								{
									free(msg);
									qDebug() << __LINE__ << " malloc failed";
								}
								else
								{
									memset(msg->data, 0, rdc.size());
                                    memcpy_s(msg->data, rdc.size(), rdc.data(), rdc.size()); //把视频帧字节数组数据封装进去data部分
									msg->len = rdc.size();
									msg->ip = ip;
                                    queue_recv.push_msg(msg); //然后插入到接收队列中等待处理
								}
							}
						}
					}
					else if (msgtype == PARTNER_JOIN || msgtype == PARTNER_EXIT || msgtype == CLOSE_CAMERA)
                    { //这几个消息是纯控制类消息，只有类型，别的啥也没有
						MESG* msg = (MESG*)malloc(sizeof(MESG));
						if (msg == NULL)
						{
							qDebug() << __LINE__ << " malloc failed";
						}
						else
						{
							memset(msg, 0, sizeof(MESG));
							msg->msg_type = msgtype;
							msg->ip = ip;
                            queue_recv.push_msg(msg); //把封装好的消息放进接收队列中等待处理
						}
					}
					else if (msgtype == AUDIO_RECV)
					{
						//解压缩
						QByteArray cc((char*)recvbuf + MSG_HEADER, data_size);
						QByteArray rc = QByteArray::fromBase64(cc);
                        QByteArray rdc = qUncompress(rc);  //音频数据也要解压缩

						if (rdc.size() > 0)
						{
							MESG* msg = (MESG*)malloc(sizeof(MESG));
							if (msg == NULL)
							{
								qDebug() << __LINE__ << "malloc failed";
							}
							else
							{
								memset(msg, 0, sizeof(MESG));
                                msg->msg_type = AUDIO_RECV;    //然后这些置零取值部分几乎一样
								msg->ip = ip;

								msg->data = (uchar*)malloc(rdc.size());
								if (msg->data == nullptr)
								{
                                    free(msg);
									qDebug() << __LINE__ << "malloc msg.data failed";
								}
								else
								{
									memset(msg->data, 0, rdc.size());
									memcpy_s(msg->data, rdc.size(), rdc.data(), rdc.size());
									msg->len = rdc.size();
									msg->ip = ip;
                                    audio_recv.push_msg(msg); //最后插入到接收队列就完事了
								}
							}
						}
					}
                    else if(msgtype == TEXT_RECV)
                    {
                        //解压缩
                        QByteArray cc((char *)recvbuf + MSG_HEADER, data_size);
                        std::string rr = qUncompress(cc).toStdString(); //虽然不知道为啥连文本消息都要压缩，但它说压就压呗
                        if(rr.size() > 0)
                        {
                            MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if (msg == NULL)
                            {
                                qDebug() << __LINE__ << "malloc failed";
                            }
                            else
                            {
                                memset(msg, 0, sizeof(MESG));
                                msg->msg_type = TEXT_RECV;          //然后这一部分还是一样的，没啥变化
                                msg->ip = ip;
                                msg->data = (uchar*)malloc(rr.size());
                                if (msg->data == nullptr)
                                {
                                    free(msg);
                                    qDebug() << __LINE__ << "malloc msg.data failed";
                                }
                                else
                                {
                                    memset(msg->data, 0, rr.size());
                                    memcpy_s(msg->data, rr.size(), rr.data(), rr.size());
                                    msg->len = rr.size();
                                    queue_recv.push_msg(msg);  //最后插入到接收队列结束
                                }
                            }
                        }
                    }
                }
			}
            else
            {
                qDebug() << "package error";
            }
			memmove_s(recvbuf, 4 * MB, recvbuf + MSG_HEADER + data_size + 1, hasrecvive - ((quint64)data_size + 1 + MSG_HEADER));
            hasrecvive -= ((quint64)data_size + 1 + MSG_HEADER);  //如果消息过长的时候，需要根据总量和偏移量进行修改，保证能全部收完(比如视频帧)
        }
        else
        {
            return;
        }
    }
}


MyTcpSocket::~MyTcpSocket()
{
    delete sendbuf;
    delete _sockThread;
}



bool MyTcpSocket::connectServer(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
    if(_socktcp == nullptr) _socktcp = new QTcpSocket(); //tcp
    _socktcp->connectToHost(ip, port.toUShort(), flag); //连接至服务器
    connect(_socktcp, SIGNAL(readyRead()), this, SLOT(recvFromSocket()), Qt::UniqueConnection); //接受数据
    //处理套接字错误
    connect(_socktcp, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorDetect(QAbstractSocket::SocketError)),Qt::UniqueConnection);

    if(_socktcp->waitForConnected(5000))
    {
        return true;
    }
    _socktcp->close(); //超时未响应
    return false;
}


bool MyTcpSocket::connectToServer(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
	_sockThread->start(); // 开启链接，与接受
	bool retVal;
	QMetaObject::invokeMethod(this, "connectServer", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, retVal),
								Q_ARG(QString, ip), Q_ARG(QString, port), Q_ARG(QIODevice::OpenModeFlag, flag));
    //通过函数调用，启动了别的线程去处理连接任务，也就是连接线程和接收数据线程是分开的

	if (retVal)
	{
        this->start() ; //写数据
		return true;
	}
	else
	{
		return false;
	}
}

QString MyTcpSocket::errorString()
{
    return _socktcp->errorString();
}

void MyTcpSocket::disconnectFromHost()
{
    //write
    if(this->isRunning())
    {
        QMutexLocker locker(&m_lock);
        m_isCanRun = false;
    }

    if(_sockThread->isRunning()) //read
    {
        _sockThread->quit();
        _sockThread->wait();
    }

    //清空发送队列，清空接受队列
    queue_send.clear();
    queue_recv.clear();
	audio_recv.clear();
}


quint32 MyTcpSocket::getlocalip()
{
    if(_socktcp->isOpen())
    {
        return _socktcp->localAddress().toIPv4Address();
    }
    else
    {
        return -1;
    }
}
