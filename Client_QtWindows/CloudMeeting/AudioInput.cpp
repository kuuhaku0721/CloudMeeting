#include "AudioInput.h"
#include "netheader.h"
#include <QAudioFormat>
#include <QDebug>
#include <QThread>

extern QUEUE_DATA<MESG> queue_send; //发送队列
extern QUEUE_DATA<MESG> queue_recv; //接收队列

AudioInput::AudioInput(QObject *parent)
	: QObject(parent)
{
    recvbuf = (char*)malloc(MB * 2);  //接收缓冲区，分配给它2M的内存
    QAudioFormat format; //音频格式
    //set format  设置音频收音格式
    format.setSampleRate(8000);
	format.setChannelCount(1);
	format.setSampleSize(16);
	format.setCodec("audio/pcm");
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setSampleType(QAudioFormat::UnSignedInt);

    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice(); //设备信息默认
    if (!info.isFormatSupported(format)) //如果格式不支持的话
	{
		qWarning() << "Default format not supported, trying to use the nearest.";
        format = info.nearestFormat(format); //就找一个相似的支持的格式出来
	}
	audio = new QAudioInput(format, this);
    connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State))); //进设置格式，设置完之后的事情交给别的函数处理

}

AudioInput::~AudioInput()
{
	delete audio;
}

void AudioInput::startCollect()
{                               //活跃状态
	if (audio->state() == QAudio::ActiveState) return;
	WRITE_LOG("start collecting audio");
    inputdevice = audio->start();  //开始采集声音
    connect(inputdevice, SIGNAL(readyRead()), this, SLOT(onreadyRead())); //然后通知槽函数可以开始读了
}

void AudioInput::stopCollect()
{                               //停止状态
	if (audio->state() == QAudio::StoppedState) return;
	disconnect(this, SLOT(onreadyRead()));
    audio->stop(); //停止采集，
	WRITE_LOG("stop collecting audio");
    inputdevice = nullptr; //IO设备置空
}

void AudioInput::onreadyRead()
{
    static int num = 0, totallen  = 0; //设置两个静态全局变量设置总数和总长度
    if (inputdevice == nullptr) return; //以防万一，万一它刚刚好进来这个函数之前设备挂了呢
    int len = inputdevice->read(recvbuf + totallen, 2 * MB - totallen); //调用IO设备的读函数读取数据到缓冲区
	if (num < 2)
	{
		totallen += len;
		num++;
		return;
	}
    totallen += len; //总长度加上这次的数据长度
	qDebug() << "totallen = " << totallen;
    MESG* msg = (MESG*)malloc(sizeof(MESG));  //就算是音频数据，它也得用自己封装的消息包
	if (msg == nullptr)
	{
		qWarning() << __LINE__ << "malloc fail";
	}
	else
	{
        memset(msg, 0, sizeof(MESG)); //使用前置零
        msg->msg_type = AUDIO_SEND; //数据类型为发送音频
		//压缩数据，转base64
		QByteArray rr(recvbuf, totallen);
		QByteArray cc = qCompress(rr).toBase64();
		msg->len = cc.size();

		msg->data = (uchar*)malloc(msg->len);
		if (msg->data == nullptr)
		{
			qWarning() << "malloc mesg.data fail";
		}
		else
		{
			memset(msg->data, 0, msg->len);
            memcpy_s(msg->data, msg->len, cc.data(), cc.size()); //将音频数据拷贝进去消息体中
            queue_send.push_msg(msg); //将消息体加入到发送队列当中
		}
	}
    totallen = 0; //一条加入队列之后两个静态变量归零掉等待下一次接收
	num = 0;
}

QString AudioInput::errorString()
{
	if (audio->error() == QAudio::OpenError)
	{
		return QString("AudioInput An error occurred opening the audio device").toUtf8();
	}
	else if (audio->error() == QAudio::IOError)
	{
		return QString("AudioInput An error occurred during read/write of audio device").toUtf8();
	}
	else if (audio->error() == QAudio::UnderrunError)
	{
		return QString("AudioInput Audio data is not being fed to the audio device at a fast enough rate").toUtf8();
	}
	else if (audio->error() == QAudio::FatalError)
	{
		return QString("AudioInput A non-recoverable error has occurred, the audio device is not usable at this time.");
	}
	else
	{
		return QString("AudioInput No errors have occurred").toUtf8();
	}
}

void AudioInput::handleStateChanged(QAudio::State newState)
{
	switch (newState)
	{
		case QAudio::StoppedState:
			if (audio->error() != QAudio::NoError)
			{
				stopCollect();
				emit audioinputerror(errorString());
			}
			else
			{
				qWarning() << "stop recording";
			}
			break;
		case QAudio::ActiveState:
			//start recording
			qWarning() << "start recording";
			break;
		default:
			//
			break;
	}
}

void AudioInput::setVolumn(int v) //会与滚动条联动
{
	qDebug() << v;
	audio->setVolume(v / 100.0);
}
