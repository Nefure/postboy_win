#include "postboy_win.h"

#include<qtimer.h>

#include"ItemButton.h"

QString postboy_win::name;
static QString SELF_PREFFIX = "本机名：";

postboy_win::postboy_win(QWidget* parent)
    : QMainWindow(parent), moving(false), diplomat(), hostRefreshed(true), mailboxes(),ui(new Ui::postboy_winClass())
{
    //初始化样式
    char hostname[256] = { 0 };
    gethostname(hostname, sizeof(hostname));
    name = hostname;

    //除去自带窗体
    this->setWindowFlags(Qt::FramelessWindowHint);
    ui->setupUi(this);

    ui->stackedWidget->setCurrentIndex(1);

    //设置默认用户名
    ui->nameInput->setText(name);
    ui->selfItem->setHidden(true);

    //初始化事件
    connect(ui->closeBtn, &QToolButton::clicked, this, &QMainWindow::close);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &QMainWindow::close);
    connect(ui->min_btn, &QToolButton::clicked, [ = ]()
    {
        this->setWindowState(Qt::WindowMinimized);
    });
    QSize size(640, 480);
    QSize old = this->size();
    connect(ui->enterBtn, &QPushButton::clicked, [ = ]()
    {
        QPoint pos =  this->pos();
        this->hide();

        ui->stackedWidget->setCurrentIndex(0);
        name = ui->nameInput->text();

        this->resize(size);
        this->move(pos.x() + size.width() / 2 - old.width(), pos.y() + size.height() / 2 - old.height());
        this->show();
    });

    connect(ui->flushBtn, &QPushButton::clicked, [ = ] { ui->stackedWidget->setCurrentIndex(1); });

    connect(ui->selfItem, &QToolButton::clicked, [ = ]()
        {
            QString text = this->ui->selfItem->text();
            communicate(ui->selfItem->getTag(),text);
    });

    diplomat.listenStart();

    //每俩秒广播一次自己信息
    QTimer* callOutTimer = new QTimer(this);
    callOutTimer->start(2000);
    connect(callOutTimer, &QTimer::timeout, [ = ]()
    {
        this->diplomat.callOut(this->name);
    });

    //每3s更新一次用户列表
    QTimer* refreshTimer = new QTimer(this);
    refreshTimer->start(3000);
    connect(refreshTimer, &QTimer::timeout, [ = ]()
    {
        refresh();
    });
}

postboy_win::~postboy_win()
{
    diplomat.shutdownlisten();
    
    for (int i = 0; i <  mailboxes.size(); i++)
    {
        if (mailboxes[i]) delete mailboxes[i];
    }
    delete ui;
}

void postboy_win::mousePressEvent(QMouseEvent* e)
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

void postboy_win::mouseMoveEvent(QMouseEvent* e)
{
    if (!moving)
    {
        return;
    }
    this->move(e->globalPos() - subPoint);
}

void postboy_win::mouseReleaseEvent(QMouseEvent* e)
{
    moving = false;
}

ItemButton* postboy_win::addUserItem()
{
    ItemButton* item = new ItemButton(ui->scrollArea);
    item->copyFrom(ui->selfItem);

    
    connect(item, &ItemButton::clicked, [ = ]() {QString text = item->text(); communicate(item->getTag(),text); });
    ui->scrollLayout->insertWidget(2, item);

    return item;
}



void postboy_win::fillUserItem(ItemButton* item, Host& newHost)
{
    item->setTag(newHost.addr);

    QString txt;
    diplomat.toHostName(txt, newHost.name, newHost.addr);
    item->setText(txt);
}

void postboy_win::refresh()
{
    //通知主机信息管理对象进行更新
    if (((!hostRefreshed) && !diplomat.refresh()))
    {
        return;
    }

    hostRefreshed = true;

    if (READING != diplomat.getRwFlag() && !diplomat.read())
    {
        return;
    }
    hostRefreshed = false;

    QVBoxLayout* layout = ui->scrollLayout;
    //把主机信息同步到视图
    std::vector<Host>& hosts = diplomat.hostCopy;
    int count = layout->count() - 1;
    //清除旧项标记
    for (int i = 1; i < count; i++)
    {
        ItemButton* item = (ItemButton*)layout->itemAt(i)->widget();
        item->setHidden(true);
        item->setTag(0);
    }
    //补足控件
    int deta = hosts.size() - count + 1;
    while (deta > 0)
    {
        addUserItem();
        deta--;
    }
    //填入标记及主机信息
    count = layout->count() - 1;
    for (int i = 1; i < count && i <= hosts.size(); i++)
    {
        ItemButton* item = (ItemButton*)layout->itemAt(i)->widget();
        fillUserItem(item, hosts[i - (int)1]);
        item->setHidden(false);
    }

    diplomat.readFinish();
}

void postboy_win::communicate(unsigned long ip,QString& name)
{
    MailboxWidgets* mailbox = NULL;
    for (int i = 0; i < mailboxes.size(); i++)
    {
        if (mailboxes[i] && mailboxes[i]->addr == ip)
        {
            mailboxes[i]->activateWindow();
            mailboxes[i]->setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
            return;
        }
    }
    mailbox = new MailboxWidgets(&diplomat,ip);
    mailbox->setTile(name);
    mailbox->show();
    mailboxes.push_back(mailbox);
}

