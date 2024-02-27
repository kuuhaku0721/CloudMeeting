#include "myvideosurface.h"
#include <QVideoSurfaceFormat>
#include <QDebug>
MyVideoSurface::MyVideoSurface(QObject *parent):QAbstractVideoSurface(parent)
{

}
                                                //支持的视频帧格式
QList<QVideoFrame::PixelFormat> MyVideoSurface::supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const
{
    if(handleType == QAbstractVideoBuffer::NoHandle) //参数handletype的默认值就是NoHandle.
    {
        return QList<QVideoFrame::PixelFormat>() << QVideoFrame::Format_RGB32
                                                 << QVideoFrame::Format_ARGB32
                                                 << QVideoFrame::Format_ARGB32_Premultiplied
                                                 << QVideoFrame::Format_RGB565
                                                 << QVideoFrame::Format_RGB555;
    }
    else
    {
        return QList<QVideoFrame::PixelFormat>();
    }
}

                   //判断格式是否支持
bool MyVideoSurface::isFormatSupported(const QVideoSurfaceFormat &format) const
{
    // imageFormatFromPixelFormat: 返回与视频帧像素格式等效的图像格式
    //pixelFormat: 返回视频流中的像素格式
    return QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat()) != QImage::Format_Invalid;
}

//将视频流中像素格式转换成格式对等的图片格式
bool MyVideoSurface::start(const QVideoSurfaceFormat &format)
{
    if(isFormatSupported(format) && !format.frameSize().isEmpty())
    {
        QAbstractVideoSurface::start(format);
        return true;
    }
    return false;
}


//捕获每一帧视频，每一帧都会到present  每一帧都要显示到画面上去
bool MyVideoSurface::present(const QVideoFrame &frame)
{
    if(!frame.isValid())
    {
        stop();
        return false;
    }
    if(frame.isMapped())
    {
        emit frameAvailable(frame);  //发射信号告知视频可以显示
    }
    else
    {
        QVideoFrame f(frame);
        f.map(QAbstractVideoBuffer::ReadOnly);
        emit frameAvailable(f);
    }

    return true;
}
