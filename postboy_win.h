#pragma once

#include<qevent.h>
#include <QtWidgets/QMainWindow>
#include "ui_postboy_win.h"
#include "diplomat.h"
#include "mailboxwidgets.h"

class postboy_win : public QMainWindow
{
    Q_OBJECT

public:
    postboy_win(QWidget *parent = nullptr);
    ~postboy_win();

    void  mousePressEvent(QMouseEvent* e);

    void mouseMoveEvent(QMouseEvent* e);

    void  mouseReleaseEvent(QMouseEvent* e);
private:

    bool hostRefreshed;

    bool moving;

    QPoint subPoint;
    Diplomat diplomat;
    static QString name;
    Ui::postboy_winClass *ui;

    std::vector<MailboxWidgets*> mailboxes;

    ItemButton* addUserItem();

    void fillUserItem(ItemButton* item , Host& newHost);

    void refresh();

    //TODO:检查并打开对应窗口，建立交流
    void communicate(unsigned long ip, QString& name);
};
