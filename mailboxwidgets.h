#pragma once

#include<qevent.h>
#include <QMainWindow>
#include<qtimer.h>
#include "ui_mailboxwidgets.h"
#include "diplomat.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MailboxWidgetsClass; };
QT_END_NAMESPACE

class MailboxWidgets : public QMainWindow
{
	Q_OBJECT

public:

    //当前连接的ip
    unsigned long addr;
    MailboxWidgets(Diplomat* diplomate,unsigned long addr,  QWidget* parent = NULL );
	~MailboxWidgets();

    void setTile(QString& title);

    void mousePressEvent(QMouseEvent* e)
    {
        QPoint click = e->globalPos();
        QPoint wind = this->pos();
        QPoint sub = click - wind;
        if (sub.y() > ui->titleBar->height() || moving)
        {
            return;
        }
        moving = true;
        subPoint = sub;
    }

    void mouseMoveEvent(QMouseEvent* e)
    {
        if (!moving)
        {
            return;
        }
        this->move(e->globalPos() - subPoint);
    }

    void mouseReleaseEvent(QMouseEvent* e)
    {
        moving = false;
    }
private:
	bool moving;
    Msg* msgs = NULL;
    QPoint subPoint;
    Diplomat* diplomat;
	Ui::MailboxWidgetsClass *ui;

    QTimer* msgTimer;
    QMetaObject::Connection findMsgConn;

    void pushMsg(QString& msg, bool right = true);

    void customTitle();

    void insertActions();

    //找已经接收的消息的消息头地址
    void findMsg();

    void updateMsg();
};
