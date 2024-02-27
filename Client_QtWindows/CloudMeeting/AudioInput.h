#pragma once

#include <QObject>
#include <QAudioInput>
#include <QIODevice>
class AudioInput : public QObject   //音频的输入，也就是录音
{
	Q_OBJECT
private:
    QAudioInput *audio;  //输入的音频数据
    QIODevice* inputdevice; //负责录入音频的IO设备
    char* recvbuf; //接收缓冲区
public:
	AudioInput(QObject *par = 0);
	~AudioInput();
private slots:
	void onreadyRead();
	void handleStateChanged(QAudio::State);
    QString errorString(); //出错提示
    void setVolumn(int);  //设置音量
public slots:
    void startCollect(); //开始采集音频数据
    void stopCollect(); //停止采集
signals:
	void audioinputerror(QString);
};
