#include "chatmessage.h"
#include <QFontMetrics>
#include <QPaintEvent>
#include <QDateTime>
#include <QPainter>
#include <QMovie> //动画效果类
#include <QLabel>
#include <QDebug>

ChatMessage::ChatMessage(QWidget *parent) : QWidget(parent)
{
    QFont te_font = this->font(); //设置发送字体
    te_font.setFamily("MicrosoftYaHei");
    te_font.setPointSize(12);
    this->setFont(te_font);
    m_leftPixmap = QPixmap(":/myImage/1.jpg"); //设置头像
    m_rightPixmap = QPixmap(":/myImage/1.jpg");

    m_loadingMovie = new QMovie(this); //设置加载动画
    m_loadingMovie->setFileName(":/myImage/3.gif"); //网络不好的时候有个加载动画
    m_loading = new QLabel(this);
    m_loading->setMovie(m_loadingMovie); //设置动画效果--选择一个动态图片
    m_loading->setScaledContents(true);
    m_loading->resize(40,40); //设置大小
    m_loading->setAttribute(Qt::WA_TranslucentBackground , true); //设置参数，一般都是默认
}

void ChatMessage::setTextSuccess()
{
    m_loading->hide(); //发送成功就不要有加载转圈圈了
    m_loadingMovie->stop(); //动画效果停止
    m_isSending = true; //发送状态设为true 标志消息发送成功
}

void ChatMessage::setText(QString text, QString time, QSize allSize, QString ip,ChatMessage::User_Type userType)
{
    m_msg = text; //消息文本
    m_userType = userType; //用户类习惯
    m_time = time; //时间
    m_curTime = QDateTime::fromTime_t(time.toInt()).toString("ddd hh:mm"); //当前时间
    m_allSize = allSize; //整个大小
    m_ip = ip; //发送者ip地址
    if(userType == User_Me) {
        if(!m_isSending) {
            //如果没有发送出去的话才会显示转圈圈加载动画
            m_loading->move(m_kuangRightRect.x() - m_loading->width() - 10, m_kuangRightRect.y()+m_kuangRightRect.height()/2- m_loading->height()/2);
//            m_loading->move(0, 0);
            m_loading->show(); //转圈圈之前先让它显示出来
            m_loadingMovie->start(); //动画效果转圈圈 开转
        }
    } else {
        m_loading->hide(); //如果是自己发的，自己看到时是有一个加载转圈圈的，转完发送成功别人才能看到，但是如果是别人发的，能看到的时候就已经发送成功了，所以不用显示加载动画
    }

    this->update(); //每有一条消息就更新一下Widget的显示 update会触发窗口重绘
}

QSize ChatMessage::fontRect(QString str)
{
    m_msg = str; //只有文字内容动态设置，其他的东西都是控制大小的，写死
    int minHei = 30;
    int iconWH = 40;
    int iconSpaceW = 20;
    int iconRectW = 5;
    int iconTMPH = 10;
    int sanJiaoW = 6;
    int kuangTMP = 20;
    int textSpaceRect = 12;
    m_kuangWidth = this->width() - kuangTMP - 2*(iconWH+iconSpaceW+iconRectW);
    m_textWidth = m_kuangWidth - 2*textSpaceRect;
    m_spaceWid = this->width() - m_textWidth;
    m_iconLeftRect = QRect(iconSpaceW, iconTMPH + 10, iconWH, iconWH);
    m_iconRightRect = QRect(this->width() - iconSpaceW - iconWH, iconTMPH + 10, iconWH, iconWH);


    QSize size = getRealString(m_msg); // 整个的size

    qDebug() << "fontRect Size:" << size;
    int hei = size.height() < minHei ? minHei : size.height(); //设置高度

    m_sanjiaoLeftRect = QRect(iconWH+iconSpaceW+iconRectW, m_lineHeight/2 + 10, sanJiaoW, hei - m_lineHeight);
    m_sanjiaoRightRect = QRect(this->width() - iconRectW - iconWH - iconSpaceW - sanJiaoW, m_lineHeight/2+10, sanJiaoW, hei - m_lineHeight);

    if(size.width() < (m_textWidth+m_spaceWid)) {
        m_kuangLeftRect.setRect(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), m_lineHeight/4*3 + 10, size.width()-m_spaceWid+2*textSpaceRect, hei-m_lineHeight);
        m_kuangRightRect.setRect(this->width() - size.width() + m_spaceWid - 2*textSpaceRect - iconWH - iconSpaceW - iconRectW - sanJiaoW,
                                 m_lineHeight/4*3 + 10, size.width()-m_spaceWid+2*textSpaceRect, hei-m_lineHeight);
    } else {
        m_kuangLeftRect.setRect(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), m_lineHeight/4*3 + 10, m_kuangWidth, hei-m_lineHeight);
        m_kuangRightRect.setRect(iconWH + kuangTMP + iconSpaceW + iconRectW - sanJiaoW, m_lineHeight/4*3 + 10, m_kuangWidth, hei-m_lineHeight);
    }
    m_textLeftRect.setRect(m_kuangLeftRect.x()+textSpaceRect, m_kuangLeftRect.y()+iconTMPH,
                           m_kuangLeftRect.width()-2*textSpaceRect, m_kuangLeftRect.height()-2*iconTMPH);
    m_textRightRect.setRect(m_kuangRightRect.x()+textSpaceRect, m_kuangRightRect.y()+iconTMPH,
                            m_kuangRightRect.width()-2*textSpaceRect, m_kuangRightRect.height()-2*iconTMPH);


    m_ipLeftRect.setRect(m_kuangLeftRect.x(), m_kuangLeftRect.y()+iconTMPH - 20,
                           m_kuangLeftRect.width()-2*textSpaceRect + iconWH*2, 20);
    m_ipRightRect.setRect(m_kuangRightRect.x(), m_kuangRightRect.y()+iconTMPH - 30,
                            m_kuangRightRect.width()-2*textSpaceRect + iconWH*2 , 20);
    return QSize(size.width(), hei + 15);
}

QSize ChatMessage::getRealString(QString src)
{
    QFontMetricsF fm(this->font()); //字体
    m_lineHeight = fm.lineSpacing(); //线高 控制行距的
    int nCount = src.count("\n"); //一共有几行
    int nMaxWidth = 0; //最大宽度,决定了每行显示多少东西
    if(nCount == 0) {   //一共只输了一行，很长的一行的时候
        nMaxWidth = fm.width(src);
        QString value = src;
        if(nMaxWidth > m_textWidth)
        {
            nMaxWidth = m_textWidth;
            int size = m_textWidth / fm.width(" ");
            int num = fm.width(value) / m_textWidth;
            num = ( fm.width(value) ) / m_textWidth;
            nCount += num;
            QString temp = ""; //控制每一行显示多少字，然后在多的部分加换行符
            for(int i = 0; i < num; i++)
            {
                temp += value.mid(i*size, (i+1)*size) + "\n";
            }
            src.replace(value, temp); //最后替换一下文本内容，把原本的长串替换成多行内容
        }
    } else { //分开输了好多行的情况
        for(int i = 0; i < (nCount + 1); i++)
        {
            QString value = src.split("\n").at(i);
            nMaxWidth = fm.width(value) > nMaxWidth ? fm.width(value) : nMaxWidth;
            if(fm.width(value) > m_textWidth)  //跟上面的差不多，只不过是一行一行的找，找那些特别长的，把它拆分成多行
            {
                nMaxWidth = m_textWidth;
                int size = m_textWidth / fm.width(" ");
                int num = fm.width(value) / m_textWidth;
                num = ((i+num)*fm.width(" ") + fm.width(value)) / m_textWidth;
                nCount += num;
                QString temp = "";
                for(int i = 0; i < num; i++)
                {
                    temp += value.mid(i*size, (i+1)*size) + "\n";
                }
                src.replace(value, temp);
            }
        }
    }
    return QSize(nMaxWidth+m_spaceWid, (nCount + 1) * m_lineHeight+2*m_lineHeight);
}

void ChatMessage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);//消锯齿
    painter.setPen(Qt::NoPen); //无画笔，就是不用划出来边框
    painter.setBrush(QBrush(Qt::gray));

    if(m_userType == User_Type::User_She)
    { // 用户
        //头像
//        painter.drawRoundedRect(m_iconLeftRect,m_iconLeftRect.width(),m_iconLeftRect.height());
        painter.drawPixmap(m_iconLeftRect, m_leftPixmap);

        //框加边
        QColor col_KuangB(234, 234, 234);
        painter.setBrush(QBrush(col_KuangB));
        painter.drawRoundedRect(m_kuangLeftRect.x()-1,m_kuangLeftRect.y()-1 + 10,m_kuangLeftRect.width()+2,m_kuangLeftRect.height()+2,4,4);
        //框
        QColor col_Kuang(255,255,255);
        painter.setBrush(QBrush(col_Kuang));
        painter.drawRoundedRect(m_kuangLeftRect,4,4);

        //三角
        QPointF points[3] = {
            QPointF(m_sanjiaoLeftRect.x(), 40),
            QPointF(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), 35),
            QPointF(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), 45),
        };
        QPen pen;
        pen.setColor(col_Kuang);
        painter.setPen(pen);
        painter.drawPolygon(points, 3);

        //三角加边
        QPen penSanJiaoBian;
        penSanJiaoBian.setColor(col_KuangB);
        painter.setPen(penSanJiaoBian);
        painter.drawLine(QPointF(m_sanjiaoLeftRect.x() - 1, 30), QPointF(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), 24));
        painter.drawLine(QPointF(m_sanjiaoLeftRect.x() - 1, 30), QPointF(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), 36));

        //ip
        //ip
        QPen penIp;
        penIp.setColor(Qt::darkGray);
        painter.setPen(penIp);
        QFont f = this->font();
        f.setPointSize(10);
        QTextOption op(Qt::AlignHCenter | Qt::AlignVCenter);
        painter.setFont(f);
        painter.drawText(m_ipLeftRect, m_ip, op);

        //内容
        QPen penText;
        penText.setColor(QColor(51,51,51));
        painter.setPen(penText);
        QTextOption option(Qt::AlignLeft | Qt::AlignVCenter); //水平左对齐，垂直居中
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.setFont(this->font()); //设置字体
        painter.drawText(m_textLeftRect, m_msg,option); //最后把文字内容显示上去
    }
    else if(m_userType == User_Type::User_Me)
    { // 自己
        //头像
//      painter.drawRoundedRect(m_iconRightRect,m_iconRightRect.width(),m_iconRightRect.height());
        painter.drawPixmap(m_iconRightRect, m_rightPixmap);

        //框
        QColor col_Kuang(75,164,242);
        painter.setBrush(QBrush(col_Kuang));
        painter.drawRoundedRect(m_kuangRightRect,4,4);

        //三角
        QPointF points[3] = {
            QPointF(m_sanjiaoRightRect.x()+m_sanjiaoRightRect.width(), 40),
            QPointF(m_sanjiaoRightRect.x(), 35),
            QPointF(m_sanjiaoRightRect.x(), 45),
        };
        QPen pen;
        pen.setColor(col_Kuang);
        painter.setPen(pen);
        painter.drawPolygon(points, 3);


        //ip
        QPen penIp;
        penIp.setColor(Qt::black);
        painter.setPen(penIp);
        QFont f = this->font();
        f.setPointSize(10);
        QTextOption op(Qt::AlignHCenter | Qt::AlignVCenter);
        painter.setFont(f);
        painter.drawText(m_ipRightRect, m_ip, op);

        //内容
        QPen penText;
        penText.setColor(Qt::white);
        painter.setPen(penText);
        QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.setFont(this->font());
        painter.drawText(m_textRightRect,m_msg,option);
    }
    else if(m_userType == User_Type::User_Time)
    { // 时间
        QPen penText;
        penText.setColor(QColor(153,153,153));
        painter.setPen(penText);
        QTextOption option(Qt::AlignCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        QFont te_font = this->font();
        te_font.setFamily("MicrosoftYaHei");
        te_font.setPointSize(10);
        painter.setFont(te_font);
        painter.drawText(this->rect(),m_curTime,option);
    }
};
