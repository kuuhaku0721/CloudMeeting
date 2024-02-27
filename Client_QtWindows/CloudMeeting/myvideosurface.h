#ifndef MYVIDEOSURFACE_H
#define MYVIDEOSURFACE_H

#include <QAbstractVideoSurface>


class MyVideoSurface : public QAbstractVideoSurface  //负责控制会议时视频功能的
{
    Q_OBJECT
public:
    MyVideoSurface(QObject *parent = 0);

    //支持的像素格式  VideoFrame表示视频数据的一帧，在使用的时候是将其转换为QImage图片格式来使用(传送)的
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const override;

    //检测视频流的格式是否合法，返回bool
    bool isFormatSupported(const QVideoSurfaceFormat &format) const override;
    bool start(const QVideoSurfaceFormat &format) override; //启动
    bool present(const QVideoFrame &frame) override; //显示
    QRect videoRect() const; //视频区域
    void updateVideoRect(); //更新窗口
    void paint(QPainter *painter); //绘图事件

//private:
//    QWidget      * widget_;
//    QImage::Format imageFormat_;
//    QRect          targetRect_;
//    QSize          imageSize_;
//    QVideoFrame    currentFrame_;

signals:
    void frameAvailable(QVideoFrame);   //信号关联的函数在Widget里面 涉及到cameraImageCapture

};

#endif // MYVIDEOSURFACE_H
