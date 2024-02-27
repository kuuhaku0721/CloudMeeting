#ifndef PARTNER_H
#define PARTNER_H

#include <QLabel>

class Partner : public QLabel   //与会成员是以label的形式显示在了ListWidget上
{
    Q_OBJECT
private:
    quint32 ip;

    void mousePressEvent(QMouseEvent *ev) override;
    int w;
public:
    Partner(QWidget * parent = nullptr, quint32 = 0);
    void setpic(QImage img); //设置label的图片
signals:
    void sendip(quint32); //发送ip
};

#endif // PARTNER_H
