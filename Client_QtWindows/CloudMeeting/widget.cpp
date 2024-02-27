#include "widget.h"
#include "ui_widget.h"
#include "screen.h"
#include <QCamera>
#include <QCameraViewfinder>
#include <QCameraImageCapture>
#include <QDebug>
#include <QPainter>
#include "myvideosurface.h"
#include "sendimg.h"
#include <QRegExp>  //正则表达式
#include <QRegExpValidator> //正则表达式验证
#include <QMessageBox>
#include <QScrollBar>
#include <QHostAddress>
#include <QTextCodec>
#include "logqueue.h"
#include <QDateTime>
#include <QCompleter>
#include <QStringListModel>
#include <QSound>
QRect  Widget::pos = QRect(-1, -1, -1, -1); //获取当前屏幕中焦点位置，参数为上下左右

extern LogQueue *logqueue; //实例化一个日志队列，在一切开始之前，先启动日志服务

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{


    //开启日志线程
    logqueue = new LogQueue(); //实例化一个日志队列
    logqueue->start(); //日志类继承自QThread线程类，调用线程的默认start函数，让日志线程也跑起来，从程序启动开始就准备好写日志

    qDebug() << "main: " <<QThread::currentThread();
    qRegisterMetaType<MSG_TYPE>(); //qRegisterMetaType是将MSG_TYPE注册为一个元对象(比如QString)，能够在运行时创建和销毁，方便使用

    WRITE_LOG("-------------------------Application Start---------------------------"); //WRITE_LOG是以宏的形式简化了日志函数调用，后面不再赘述
    WRITE_LOG("main UI thread id: 0x%p", QThread::currentThreadId());
    //ui界面
    _createmeet = false;
    _openCamera = false;
    _joinmeet = false;  //这几个bool值初始都设置为false，总不能程序一开始就啥都要干吧
    Widget::pos = QRect(0.1 * Screen::width, 0.1 * Screen::height, 0.8 * Screen::width, 0.8 * Screen::height); //这个pos就是从主屏幕划了一块区域出来，当作主窗口位置用的

    ui->setupUi(this);

    ui->openAudio->setText(QString(OPENAUDIO).toUtf8()); //用这种蹩脚的方式去设置UI界面的文本内容主要还是防止乱码
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());

    this->setGeometry(Widget::pos); //设置当前窗口所占位置
    this->setMinimumSize(QSize(Widget::pos.width() * 0.7, Widget::pos.height() * 0.7)); //设置窗口最小大小
    this->setMaximumSize(QSize(Widget::pos.width(), Widget::pos.height())); //设置最大大小


    ui->exitmeetBtn->setDisabled(true);  //设置几乎所有的带操作按钮都不可用
    ui->joinmeetBtn->setDisabled(true);
    ui->createmeetBtn->setDisabled(true);
    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->sendmsg->setDisabled(true);
    mainip = 0; //主屏幕显示的用户IP图像

    //-------------------局部线程----------------------------
    //创建传输视频帧线程
    _sendImg = new SendImg();
    _imgThread = new QThread();
    _sendImg->moveToThread(_imgThread); //新起线程接受视频帧 moveToThread是另一种使用线程的方法，是将sendImg的事件循环全部交给新的QThread对象imgThread去处理
    _sendImg->start();
    //_imgThread->start();
    //处理每一帧数据

    //--------------------------------------------------


    //数据处理（局部线程）
    _mytcpSocket = new MyTcpSocket(); // 底层线程专管发送
    connect(_mytcpSocket, SIGNAL(sendTextOver()), this, SLOT(textSend()));
    //connect(_mytcpSocket, SIGNAL(socketerror(QAbstractSocket::SocketError)), this, SLOT(mytcperror(QAbstractSocket::SocketError)));


    //----------------------------------------------------------
    //文本传输(局部线程)
    _sendText = new SendText();
    _textThread = new QThread();
    _sendText->moveToThread(_textThread);
    _textThread->start(); // 加入线程
    _sendText->start(); // 发送

    connect(this, SIGNAL(PushText(MSG_TYPE,QString)), _sendText, SLOT(push_Text(MSG_TYPE,QString)));
    //-----------------------------------------------------------

    //配置摄像头
    _camera = new QCamera(this);
    //摄像头出错处理
    connect(_camera, SIGNAL(error(QCamera::Error)), this, SLOT(cameraError(QCamera::Error)));
    _imagecapture = new QCameraImageCapture(_camera); //自带的摄像头截图捕获接口
    _myvideosurface = new MyVideoSurface(this);  //负责控制视频帧与图像格式转换的，每一帧都要转换，每一帧都要显示


    connect(_myvideosurface, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(cameraImageCapture(QVideoFrame)));
    connect(this, SIGNAL(pushImg(QImage)), _sendImg, SLOT(ImageCapture(QImage)));


    //监听_imgThread退出信号
    connect(_imgThread, SIGNAL(finished()), _sendImg, SLOT(clearImgQueue())); //当线程的任务完成之后触发隐藏的finished信号，由它触发清空队列函数


    //------------------启动接收数据线程-------------------------
    _recvThread = new RecvSolve(); //接收处理线程只负责从接收队列中拿取数据，对于数据的处理交给槽函数去解决
    connect(_recvThread, SIGNAL(datarecv(MESG*)), this, SLOT(datasolve(MESG*)), Qt::BlockingQueuedConnection); //槽函数在后面
    _recvThread->start();

    //预览窗口重定向在MyVideoSurface
    _camera->setViewfinder(_myvideosurface);
    _camera->setCaptureMode(QCamera::CaptureStillImage);

    //音频
    _ainput = new AudioInput();  //音频输入，采集声音
    _ainputThread = new QThread();
    _ainput->moveToThread(_ainputThread);


    _aoutput = new AudioOutput(); //音频输出，播放声音

	_ainputThread->start(); //获取音频，发送
	_aoutput->start(); //播放

    connect(this, SIGNAL(startAudio()), _ainput, SLOT(startCollect())); //开始采集音频
    connect(this, SIGNAL(stopAudio()), _ainput, SLOT(stopCollect()));   //停止采集
    connect(_ainput, SIGNAL(audioinputerror(QString)), this, SLOT(audioError(QString))); //采集音频出错槽函数
    connect(_aoutput, SIGNAL(audiooutputerror(QString)), this, SLOT(audioError(QString))); //输出音频出错槽函数
    connect(_aoutput, SIGNAL(speaker(QString)), this, SLOT(speaks(QString))); //某某说话了

    //设置滚动条
    ui->scrollArea->verticalScrollBar()->setStyleSheet("QScrollBar:vertical { width:8px; background:rgba(0,0,0,0%); margin:0px,0px,0px,0px; padding-top:9px; padding-bottom:9px; } QScrollBar::handle:vertical { width:8px; background:rgba(0,0,0,25%); border-radius:4px; min-height:20; } QScrollBar::handle:vertical:hover { width:8px; background:rgba(0,0,0,50%); border-radius:4px; min-height:20; } QScrollBar::add-line:vertical { height:9px;width:8px; border-image:url(:/images/a/3.png); subcontrol-position:bottom; } QScrollBar::sub-line:vertical { height:9px;width:8px; border-image:url(:/images/a/1.png); subcontrol-position:top; } QScrollBar::add-line:vertical:hover { height:9px;width:8px; border-image:url(:/images/a/4.png); subcontrol-position:bottom; } QScrollBar::sub-line:vertical:hover { height:9px;width:8px; border-image:url(:/images/a/2.png); subcontrol-position:top; } QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:rgba(0,0,0,10%); border-radius:4px; }");
    ui->listWidget->setStyleSheet("QScrollBar:vertical { width:8px; background:rgba(0,0,0,0%); margin:0px,0px,0px,0px; padding-top:9px; padding-bottom:9px; } QScrollBar::handle:vertical { width:8px; background:rgba(0,0,0,25%); border-radius:4px; min-height:20; } QScrollBar::handle:vertical:hover { width:8px; background:rgba(0,0,0,50%); border-radius:4px; min-height:20; } QScrollBar::add-line:vertical { height:9px;width:8px; border-image:url(:/images/a/3.png); subcontrol-position:bottom; } QScrollBar::sub-line:vertical { height:9px;width:8px; border-image:url(:/images/a/1.png); subcontrol-position:top; } QScrollBar::add-line:vertical:hover { height:9px;width:8px; border-image:url(:/images/a/4.png); subcontrol-position:bottom; } QScrollBar::sub-line:vertical:hover { height:9px;width:8px; border-image:url(:/images/a/2.png); subcontrol-position:top; } QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:rgba(0,0,0,10%); border-radius:4px; }");

    QFont te_font = this->font();
    te_font.setFamily("MicrosoftYaHei"); //微软雅黑
    te_font.setPointSize(12);

    ui->listWidget->setFont(te_font);

    ui->tabWidget->setCurrentIndex(1);  //聊天窗口
    ui->tabWidget->setCurrentIndex(0);  //副屏幕
}


void Widget::cameraImageCapture(QVideoFrame frame)
{
//    qDebug() << QThread::currentThreadId() << this;

    if(frame.isValid() && frame.isReadable()) //保证视频窗口是可操作的
    {   //把视频帧转换成了对应格式的Image对象(位图),转换格式函数在对应的类里面写的有
        QImage videoImg = QImage(frame.bits(), frame.width(), frame.height(), QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat()));

        QTransform matrix; //矩阵转换
        matrix.rotate(180.0);
        //把视频帧保存下来到img对象                                            //这个label就是视频窗口区
        QImage img =  videoImg.transformed(matrix, Qt::FastTransformation).scaled(ui->mainshow_label->size());

        if(partner.size() > 1) //如果会议室内有其他成员
        {
            emit pushImg(img); //发射推送视频的信号（视频就是一帧又一帧的对象）
        }

        if(_mytcpSocket->getlocalip() == mainip) //在自己的屏幕上也要显示出来视频画面
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshow_label->size()));
        }

        Partner *p = partner[_mytcpSocket->getlocalip()];  //设置上会议成员的视频画面
        if(p) p->setpic(img);

        //qDebug()<< "format: " <<  videoImg.format() << "size: " << videoImg.size() << "byteSIze: "<< videoImg.sizeInBytes();
    }
    frame.unmap();
}

Widget::~Widget()
{
    //终止底层发送与接收线程

    if(_mytcpSocket->isRunning()) //关掉套接字连接
    {
        _mytcpSocket->stopImmediately();
        _mytcpSocket->wait();
    }

    //终止接收处理线程
    if(_recvThread->isRunning()) //关掉接收线程
    {
        _recvThread->stopImmediately();
        _recvThread->wait();
    }

    if(_imgThread->isRunning()) //关掉截取图片线程
    {
        _imgThread->quit();
        _imgThread->wait();
    }

    if(_sendImg->isRunning()) //关掉发送图片线程
    {
        _sendImg->stopImmediately();
        _sendImg->wait();
    }

    if(_textThread->isRunning()) //关掉文本处理线程
    {
        _textThread->quit();
        _textThread->wait();
    }

    if(_sendText->isRunning()) //关掉发送文字线程
    {
        _sendText->stopImmediately();
        _sendText->wait();
    }
    
    if (_ainputThread->isRunning()) //关掉音频输入线程
    {
        _ainputThread->quit();
        _ainputThread->wait();
    }

    if (_aoutput->isRunning())  //关掉文本输出线程
    {
        _aoutput->stopImmediately();
        _aoutput->wait();
    }
    WRITE_LOG("-------------------Application End-----------------");

    //关闭日志
    if(logqueue->isRunning())  //最后再关日志线程   保证日志线程最早开启最晚离开
    {
        logqueue->stopImmediately();
        logqueue->wait();
    }

    delete ui;
}

void Widget::on_createmeetBtn_clicked()
{ //创建会议按钮
    if(false == _createmeet)
    {
        ui->createmeetBtn->setDisabled(true);  //设置一下其他按钮的状态
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        ui->exitmeetBtn->setDisabled(true);
        emit PushText(CREATE_MEETING); //将 “创建会议"加入到发送队列  所有的事件都是发送到队列中，然后由队列统一处理
        WRITE_LOG("create meeting");
    }
}

void Widget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event); //未使用的，就是用父类默认的
    /*
     * 触发事件(3条， 一般使用第二条进行触发)
     * 1. 窗口部件第一次显示时，系统会自动产生一个绘图事件。从而强制绘制这个窗口部件，主窗口起来会绘制一次
     * 2. 当重新调整窗口部件的大小时，系统也会产生一个绘制事件--QWidget::update()或者QWidget::repaint()
     * 3. 当窗口部件被其它窗口部件遮挡，然后又再次显示出来时，就会对那些隐藏的区域产生一个绘制事件
    */
}


//退出会议（1，加入的会议， 2，自己创建的会议）
void Widget::on_exitmeetBtn_clicked()
{
    if(_camera->status() == QCamera::ActiveStatus) //如果摄像头开着，在离开会议时要关掉摄像头
    {
        _camera->stop();
    }

    ui->createmeetBtn->setDisabled(true); //设置一下按钮状态
    ui->exitmeetBtn->setDisabled(true);
    _createmeet = false; //退出会议时创建会议和加入会议的状态都设为false
    _joinmeet = false;
    //-----------------------------------------
    //清空partner
    clearPartner();
    // 关闭套接字

    //关闭socket
    _mytcpSocket->disconnectFromHost();
    _mytcpSocket->wait(); //从析构函数也能看出来，断掉连接之后都要wait一下，让系统去清理一下残留资源(TCB)

    ui->outlog->setText(tr("已退出会议"));

    ui->connServer->setDisabled(false); //连接至服务器的按钮状态
    ui->groupBox_2->setTitle(QString("主屏幕"));
//    ui->groupBox->setTitle(QString("副屏幕"));
    //清除聊天记录,聊天内容是显示在左侧ListWidget上的
    while(ui->listWidget->count() > 0)
    {   //从ListWidget中一条一条的取，然后一条一条的删，避免出现内存泄露
        QListWidgetItem *item = ui->listWidget->takeItem(0);
        ChatMessage *chat = (ChatMessage *) ui->listWidget->itemWidget(item);
        delete item;
        delete chat;
    }
    iplist.clear(); //iplist是现在所有的与会成员,用在Completer中输入@之后会有一个自动提示
    ui->plainTextEdit->setCompleter(iplist);


    WRITE_LOG("exit meeting");

    QMessageBox::warning(this, "Information", "退出会议" , QMessageBox::Yes, QMessageBox::Yes);

    //-----------------------------------------
}

void Widget::on_openVedio_clicked()
{
    if(_camera->status() == QCamera::ActiveStatus) //如果摄像头是开着的，再次点击就是关闭
    {
        _camera->stop(); //停止摄像头的运行
        WRITE_LOG("close camera");
        if(_camera->error() == QCamera::NoError)
        {
            _imgThread->quit(); //退出视频截图线程
            _imgThread->wait(); //wait用于清除TCB资源
            ui->openVedio->setText("开启摄像头"); //改一下按钮的文本显示
            emit PushText(CLOSE_CAMERA);
        }
        closeImg(_mytcpSocket->getlocalip());
    }
    else
    { //如果摄像头原本是关着的，再次点击就是开启
        _camera->start(); //开启摄像头
        WRITE_LOG("open camera");
        if(_camera->error() == QCamera::NoError)
        {
            _imgThread->start(); //摄像头打开之后就需要同步开启视频帧截图线程
            ui->openVedio->setText("关闭摄像头");
        }
    }
}


void Widget::on_openAudio_clicked()
{
    if (!_createmeet && !_joinmeet) return; //保证会议启动并且有俩人以上（一个房主一个用户,不然声音采集了没地方发，这一点跟视频不太一样）
    if (ui->openAudio->text().toUtf8() == QString(OPENAUDIO).toUtf8())
    {
        emit startAudio(); //发射信号，开启音频采集
        ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
    }
    else if(ui->openAudio->text().toUtf8() == QString(CLOSEAUDIO).toUtf8())
    {
        emit stopAudio(); //发射信号，停止音频的采集线程
        ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    }
}

void Widget::closeImg(quint32 ip)
{ //关闭摄像头消息对应的响应函数
    if (!partner.contains(ip))
    {
        qDebug() << "close img error";
        return;
    }
    Partner * p = partner[ip]; //这个是左侧的共享画面设置它的显示图片
    p->setpic(QImage(":/myImage/1.jpg"));  //关闭摄像头之后原本应该是显示摄像头画面的地方会把默认头像显示进去

    if(mainip == ip)
    {   //原本的画面主屏幕，关闭摄像头之后也要显示默认头像
        ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
    }
}

void Widget::on_connServer_clicked()
{
    QString ip = ui->ip->text(), port = ui->port->text();
    ui->outlog->setText("正在连接到" + ip + ":" + port);
    repaint();
    //IP的正则表达式
    QRegExp ipreg("((2{2}[0-3]|2[01][0-9]|1[0-9]{2}|0?[1-9][0-9]|0{0,2}[1-9])\\.)((25[0-5]|2[0-4][0-9]|[01]?[0-9]{0,2})\\.){2}(25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})");
    //端口的正则表达式
    QRegExp portreg("^([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])$");
    QRegExpValidator ipvalidate(ipreg), portvalidate(portreg); //验证正则表达式是否正确
    int pos = 0;
    if(ipvalidate.validate(ip, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "Input Error", "Ip Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }
    if(portvalidate.validate(port, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "Input Error", "Port Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }

    if(_mytcpSocket ->connectToServer(ip, port, QIODevice::ReadWrite))
    {
        ui->outlog->setText("成功连接到" + ip + ":" + port); //连接成功
        ui->openAudio->setDisabled(true); //更改按钮状态，连接服务器成功之后就可以继续后续操作，比如创建会议
        ui->openVedio->setDisabled(true);
        ui->createmeetBtn->setDisabled(false);
        ui->exitmeetBtn->setDisabled(true);
        ui->joinmeetBtn->setDisabled(false);
        WRITE_LOG("succeeed connecting to %s:%s", ip.toStdString().c_str(), port.toStdString().c_str());
        QMessageBox::warning(this, "Connection success", "成功连接服务器" , QMessageBox::Yes, QMessageBox::Yes);
        ui->sendmsg->setDisabled(false);
        ui->connServer->setDisabled(true);
    }
    else
    {
        ui->outlog->setText("连接失败,请重新连接...");
        WRITE_LOG("failed to connenct %s:%s", ip.toStdString().c_str(), port.toStdString().c_str());
        QMessageBox::warning(this, "Connection error", _mytcpSocket->errorString() , QMessageBox::Yes, QMessageBox::Yes);
    }
}


void Widget::cameraError(QCamera::Error)
{
    QMessageBox::warning(this, "Camera error", _camera->errorString() , QMessageBox::Yes, QMessageBox::Yes);
}

void Widget::audioError(QString err)
{
    QMessageBox::warning(this, "Audio error", err, QMessageBox::Yes);
}
//处理服务器发回来的回复数据
void Widget::datasolve(MESG *msg)
{
    if(msg->msg_type == CREATE_MEETING_RESPONSE)
    {   //创建会议的回复
        int roomno; //保存房间号的
        memcpy(&roomno, msg->data, msg->len);

        if(roomno != 0)
        {
            QMessageBox::information(this, "Room No", QString("房间号：%1").arg(roomno), QMessageBox::Yes, QMessageBox::Yes);

            ui->groupBox_2->setTitle(QString("主屏幕(房间号: %1)").arg(roomno));
            ui->outlog->setText(QString("创建成功 房间号: %1").arg(roomno) );
            _createmeet = true; //创建会议成功，更改标志以便其他函数使用
            ui->exitmeetBtn->setDisabled(false);
            ui->openVedio->setDisabled(false);
            ui->joinmeetBtn->setDisabled(true);

            WRITE_LOG("succeed creating room %d", roomno);
            //添加用户自己,创建会议室的就是房主，也是默认的第一个用户
            addPartner(_mytcpSocket->getlocalip());
            mainip = _mytcpSocket->getlocalip();
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
        }
        else
        { //服务器发回来的回复信息里面的房间号是空的，说明当前房间全满(服务端那边进程全都被占用)
            _createmeet = false;
            QMessageBox::information(this, "Room Information", QString("无可用房间"), QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("无可用房间"));
            ui->createmeetBtn->setDisabled(false);
            WRITE_LOG("no empty room");
        }
    }
    else if(msg->msg_type == JOIN_MEETING_RESPONSE)
    {   //加入房间的回复，加入房间是客户端向服务端发送房间号，然后服务端发回来加入状态，是成功还是不成功
        qint32 c;
        memcpy(&c, msg->data, msg->len);
        if(c == 0)
        {
            QMessageBox::information(this, "Meeting Error", tr("会议不存在") , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("会议不存在"));
            WRITE_LOG("meeting not exist");
            ui->exitmeetBtn->setDisabled(true); //同步更改按钮状态
            ui->openVedio->setDisabled(true);
            ui->joinmeetBtn->setDisabled(false);
            ui->connServer->setDisabled(true);
            _joinmeet = false;
        }
        else if(c == -1)
        {
            QMessageBox::warning(this, "Meeting information", "成员已满，无法加入" , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("成员已满，无法加入"));
            WRITE_LOG("full room, cannot join");
        }
        else if (c > 0)
        {
            QMessageBox::warning(this, "Meeting information", "加入成功" , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("加入成功"));
            WRITE_LOG("succeed joining room");
            //添加用户自己，这里思考一下多客户机并发的使用场景就能明白了，加入会议也要添加上用户自己
            addPartner(_mytcpSocket->getlocalip());
            mainip = _mytcpSocket->getlocalip();
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
            ui->joinmeetBtn->setDisabled(true);
            ui->exitmeetBtn->setDisabled(false);
            ui->createmeetBtn->setDisabled(true);
            _joinmeet = true; //设置加入会议状态为true，方便其他函数调用作判断条件
        }
    }
    else if(msg->msg_type == IMG_RECV)
    {   //接收到图片，就是有其他人开了摄像头，之后同步启动了发送视频帧线程，将视频帧发给了服务器，然后服务器将视频帧发给与会所有成员
        QHostAddress a(msg->ip);
        qDebug() << a.toString();
        QImage img;
        img.loadFromData(msg->data, msg->len); //加载图片
        if(partner.count(msg->ip) == 1) //只有自己的情况，就是房主自己开了摄像头，一般测试就是这个情况
        {
            Partner* p = partner[msg->ip];
            p->setpic(img);
        }
        else
        {
            Partner* p = addPartner(msg->ip);
            p->setpic(img);
        }

        if(msg->ip == mainip)
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshow_label->size())); //视频框显示出来图片
        }
        repaint(); //触发重绘事件，有图片更新就重新显示
    }
    else if(msg->msg_type == TEXT_RECV)
    {
        QString str = QString::fromStdString(std::string((char *)msg->data, msg->len));
        //qDebug() << str;
        QString time = QString::number(QDateTime::currentDateTimeUtc().toTime_t());
        ChatMessage *message = new ChatMessage(ui->listWidget);
        QListWidgetItem *item = new QListWidgetItem();
        dealMessageTime(time); //发送时间
        dealMessage(message, item, str, time, QHostAddress(msg->ip).toString() ,ChatMessage::User_She); //发送的文本
        if(str.contains('@' + QHostAddress(_mytcpSocket->getlocalip()).toString()))
        {
            QSound::play(":/myEffect/2.wav");  //收到新消息会有一个提示音
        }
    }
    else if(msg->msg_type == PARTNER_JOIN)
    { //有新成员加入的消息
        Partner* p = addPartner(msg->ip);
        if(p)
        {
            p->setpic(QImage(":/myImage/1.jpg")); //设置头像
            ui->outlog->setText(QString("%1 join meeting").arg(QHostAddress(msg->ip).toString())); //显示操作结果
            iplist.append(QString("@") + QHostAddress(msg->ip).toString()); //列表添加一个用于发消息用
            ui->plainTextEdit->setCompleter(iplist); //输入@之后的自动提示器
        }
    }
    else if(msg->msg_type == PARTNER_EXIT)
    { //有人退出会议
        removePartner(msg->ip); //移除掉这个ip对应的成员
        if(mainip == msg->ip)
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
        }
        if(iplist.removeOne(QString("@") + QHostAddress(msg->ip).toString()))
        {
            ui->plainTextEdit->setCompleter(iplist); //从提示器里删除掉这个ip
        }
        else
        {
            qDebug() << QHostAddress(msg->ip).toString() << "not exist";
            WRITE_LOG("%s not exist",QHostAddress(msg->ip).toString().toStdString().c_str());
        }
        ui->outlog->setText(QString("%1 exit meeting").arg(QHostAddress(msg->ip).toString()));
    }
    else if (msg->msg_type == CLOSE_CAMERA)
    {
        closeImg(msg->ip); //关闭摄像头之后原本显示视频画面的地方改为显示默认头像
    }
    else if (msg->msg_type == PARTNER_JOIN2)
    { //有人加入的广播消息，通知所有人有新成员加入
        uint32_t ip;
        int other = msg->len / sizeof(uint32_t), pos = 0;
        for (int i = 0; i < other; i++)
        {
            memcpy_s(&ip, sizeof(uint32_t), msg->data + pos , sizeof(uint32_t));
            pos += sizeof(uint32_t);
			Partner* p = addPartner(ip);
            if (p)
            {
                p->setpic(QImage(":/myImage/1.jpg"));
                iplist << QString("@") + QHostAddress(ip).toString();
            }
        }
        ui->plainTextEdit->setCompleter(iplist);
        ui->openVedio->setDisabled(false);
    }
    else if(msg->msg_type == RemoteHostClosedError)
    {   //服务器挂了
        clearPartner(); //清除掉所有成员
        _mytcpSocket->disconnectFromHost(); //关闭套接字连接
        _mytcpSocket->wait(); //清理残留资源
        ui->outlog->setText(QString("关闭与服务器的连接"));
        ui->createmeetBtn->setDisabled(true); //设置按钮状态
        ui->exitmeetBtn->setDisabled(true);
        ui->connServer->setDisabled(false);
        ui->joinmeetBtn->setDisabled(true);
        //清除聊天记录，因为每一条消息都是一个Widget，所以要循环一个个删除
        while(ui->listWidget->count() > 0)
        {
            QListWidgetItem *item = ui->listWidget->takeItem(0);
            ChatMessage *chat = (ChatMessage *)ui->listWidget->itemWidget(item);
            delete item;
            delete chat;
        }
        iplist.clear(); //ip列表也清空
        ui->plainTextEdit->setCompleter(iplist);//然后清空掉自动提示器
        if(_createmeet || _joinmeet) QMessageBox::warning(this, "Meeting Information", "会议结束" , QMessageBox::Yes, QMessageBox::Yes);
    }
    else if(msg->msg_type == OtherNetError)
    {  //其他错误也差不多的处理
        QMessageBox::warning(NULL, "Network Error", "网络异常" , QMessageBox::Yes, QMessageBox::Yes);
        clearPartner();
        _mytcpSocket->disconnectFromHost();
        _mytcpSocket->wait();
        ui->outlog->setText(QString("网络异常......"));
    }
    if(msg->data) //消息接收并处理完毕之后释放消息体的控件，等待接收下一次消息
    {
        free(msg->data);
        msg->data = NULL;
    }
    if(msg)
    {
        free(msg);
        msg = NULL;
    }
}

Partner* Widget::addPartner(quint32 ip)
{   //添加新成员
	if (partner.contains(ip)) return NULL;
    Partner *p = new Partner(ui->scrollAreaWidgetContents ,ip);
    if (p == NULL)
    {
        qDebug() << "new Partner error";
        return NULL;
    }
    else
    {
		connect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));
		partner.insert(ip, p);
		ui->verticalLayout_3->addWidget(p, 1);

        //当有人员加入时，开启滑动条滑动事件，开启输入(只有自己时，不打开)  只有在除房主外有其他人也在的情况下才能开启音频，打开音频之后才能设置音量
        if (partner.size() > 1)
        {
			connect(this, SIGNAL(volumnChange(int)), _ainput, SLOT(setVolumn(int)), Qt::UniqueConnection);
			connect(this, SIGNAL(volumnChange(int)), _aoutput, SLOT(setVolumn(int)), Qt::UniqueConnection);
            ui->openAudio->setDisabled(false);
            ui->sendmsg->setDisabled(false);
            _aoutput->startPlay(); //音频播放线程开启
        }
		return p;
    }
}

void Widget::removePartner(quint32 ip)
{
    if(partner.contains(ip))
    {
        Partner *p = partner[ip];
        disconnect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));  //断开用户连接
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        partner.remove(ip);

        //只有自已一个人时，关闭传输音频
        if (partner.size() <= 1)
        {
			disconnect(_ainput, SLOT(setVolumn(int)));
			disconnect(_aoutput, SLOT(setVolumn(int)));
			_ainput->stopCollect();
            _aoutput->stopPlay();
            ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
            ui->openAudio->setDisabled(true);
        }
    }
}

void Widget::clearPartner()
{   //清空所有用户，这个会在程序退出或服务端退出的情况下触发
    ui->mainshow_label->setPixmap(QPixmap());
    if(partner.size() == 0) return;

    QMap<quint32, Partner*>::iterator iter =   partner.begin();
    while (iter != partner.end()) //遍历所有用户，然后一个个删除
    {
        quint32 ip = iter.key();
        iter++;
        Partner *p =  partner.take(ip);
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        p = nullptr;
    }

    //关闭传输音频
	disconnect(_ainput, SLOT(setVolumn(int)));
    disconnect(_aoutput, SLOT(setVolumn(int)));
    //关闭音频播放与采集
	_ainput->stopCollect();
    _aoutput->stopPlay();
	ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
	ui->openAudio->setDisabled(true);
    

    //关闭图片传输线程
    if(_imgThread->isRunning())
    {
        _imgThread->quit();
        _imgThread->wait();
    }
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openVedio->setDisabled(true);
}

void Widget::recvip(quint32 ip)
{   //控制显示在左侧聊天框的显示效果
    if (partner.contains(mainip)) //自己
    {
        Partner* p = partner[mainip];                                           //显示出来的颜色不一样
        p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0 , 255, 0.7)");
    }
    if (partner.contains(ip)) //用户
	{
		Partner* p = partner[ip];
		p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(255, 0 , 0, 0.7)");
	}
	ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
    mainip = ip;
    ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
    qDebug() << ip;
}

/*
 * 加入会议
 */

void Widget::on_joinmeetBtn_clicked()
{   //加入会议按钮
    QString roomNo = ui->meetno->text(); //获取到输入的房间号，要发送给服务端的

    QRegExp roomreg("^[1-9][0-9]{1,4}$"); //用正则表达式判断正确性
    QRegExpValidator  roomvalidate(roomreg);
    int pos = 0;
    if(roomvalidate.validate(roomNo, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "RoomNo Error", "房间号不合法" , QMessageBox::Yes, QMessageBox::Yes);
    }
    else
    {
        //加入发送队列
        emit PushText(JOIN_MEETING, roomNo); //准备发给服务端
    }
}

void Widget::on_horizontalSlider_valueChanged(int value)
{
    emit volumnChange(value); //设置音量
}

void Widget::speaks(QString ip)
{
    ui->outlog->setText(QString(ip + " 正在说话").toUtf8());
}

void Widget::on_sendmsg_clicked()
{   //点击发送按钮槽函数
    QString msg = ui->plainTextEdit->toPlainText().trimmed();  //获取用户输入的文本，去除头尾空格
    if(msg.size() == 0)
    {
        qDebug() << "empty";
        return;
    }
    qDebug()<<msg;
    ui->plainTextEdit->setPlainText(""); //点完发送之后文本框置空，显示在上方区域，不过这个功能是别的函数去实现的
    QString time = QString::number(QDateTime::currentDateTimeUtc().toTime_t()); //时间
    ChatMessage *message = new ChatMessage(ui->listWidget); //聊天框列表
    QListWidgetItem *item = new QListWidgetItem();
    dealMessageTime(time);
    dealMessage(message, item, msg, time, QHostAddress(_mytcpSocket->getlocalip()).toString() ,ChatMessage::User_Me);
    emit PushText(TEXT_SEND, msg); //加入发送队列，消息也要经过服务端的中转
    ui->sendmsg->setDisabled(true);
}

void Widget::dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type)
{
    ui->listWidget->addItem(item); //聊天框是一个ListWidget，每一条消息都是一个Widget的Item，所以用addItem添加进去新消息
    messageW->setFixedWidth(ui->listWidget->width()); //这条消息的宽度
    QSize size = messageW->fontRect(text); //设置消息应该显示的大小
    item->setSizeHint(size);
    messageW->setText(text, time, size, ip, type); //最后设置一下文本内容
    ui->listWidget->setItemWidget(item, messageW); //然后添加进去就行了
}

void Widget::dealMessageTime(QString curMsgTime) //处理时间是为了：当第一次发消息时会在最上面显示发送的时间，如果连续发送的话时间不会更新，如果隔一会再发送的话就是再显示一次时间
{
    bool isShowTime = false; //决定是否要显示一下时间
    if(ui->listWidget->count() > 0)
    {
        QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);
        ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
        int lastTime = messageW->time().toInt(); //获取最后一条消息的时间
        int curTime = curMsgTime.toInt(); //再获取一下当前的时间
        qDebug() << "curTime lastTime:" << curTime - lastTime;
        isShowTime = ((curTime - lastTime) > 60); //如果两个消息相差一分钟才显示，否则不用显示
//        isShowTime = true;
    } else
    { //else的情况是第一次发消息，第一条消息一定要显示时间
        isShowTime = true;
    }
    if(isShowTime)
    {   //如果经过上面的判断之后确实需要显示一下时间
        ChatMessage* messageTime = new ChatMessage(ui->listWidget);
        QListWidgetItem* itemTime = new QListWidgetItem();
        ui->listWidget->addItem(itemTime); //添加一个时间上去
        QSize size = QSize(ui->listWidget->width() , 40); //控制一下时间显示的格式
        messageTime->resize(size);
        itemTime->setSizeHint(size);
        messageTime->setText(curMsgTime, curMsgTime, size);
        ui->listWidget->setItemWidget(itemTime, messageTime);
    }
}

void Widget::textSend()
{
    qDebug() << "send text over";
    QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);
    ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
    messageW->setTextSuccess();
    ui->sendmsg->setDisabled(false);
}
