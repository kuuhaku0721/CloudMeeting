#pragma once

#include <QObject>
#include <QThread>
#include <QAudioOutput>
#include <QMutex>
class AudioOutput : public QThread
{
	Q_OBJECT
private:
	QAudioOutput* audio;
	QIODevice* outputdevice;
	QMutex device_lock;
	
	volatile bool is_canRun;
	QMutex m_lock;
    void run(); //线程主函数
	QString errorString();
public:
	AudioOutput(QObject *parent = 0);
	~AudioOutput();
	void stopImmediately();
    void startPlay();       //音频的播放和采集差距并不大，只不过一个是开始采集，一个是开始播放
	void stopPlay();
private slots:
	void handleStateChanged(QAudio::State);
	void setVolumn(int);
	void clearQueue();
signals:
	void audiooutputerror(QString);
	void speaker(QString);
};
