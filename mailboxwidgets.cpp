#include "mailboxwidgets.h"
#include "msgWidget.h"
#include<qfiledialog.h>

MailboxWidgets::MailboxWidgets(Diplomat* diplomate, unsigned long _addr, QWidget* parent): diplomat(diplomate), addr(_addr), QMainWindow(parent), ui(new Ui::MailboxWidgetsClass()), moving(false), subPoint(), msgTimer(new QTimer(this))
{
    customTitle();

    insertActions();

    //定时拉取收到消息
    //if (addr == diplomate->getIp())return;
    msgTimer = new QTimer(this);
    msgTimer->start(50);
    findMsgConn = connect(msgTimer, &QTimer::timeout, this, &MailboxWidgets::findMsg);

}

MailboxWidgets::~MailboxWidgets()
{
    delete ui;
}

void MailboxWidgets::setTile(QString& title)
{
    ui->titleLabel->setText(title);
}

void MailboxWidgets::pushMsg(QString& msg, bool right)
{
    int cnt = ui->msgLayout->count() - 1;
    MsgWidget* msgwidget = new MsgWidget(msg,diplomat,addr, this, right);
    ui->msgLayout->insertWidget(cnt < 0 ? 0 : cnt, msgwidget);
}

void MailboxWidgets::customTitle()
{
    //除去自带窗体
    this->setWindowFlags(Qt::FramelessWindowHint);
    ui->setupUi(this);
    //标题栏初始化

    connect(ui->minBtn, &QToolButton::clicked, [ = ]()
    {
        this->setWindowState(Qt::WindowMinimized);
    });
    connect(ui->closeBtn, &QToolButton::clicked, this, &QMainWindow::close);
    connect(ui->exitBtn, &QPushButton::clicked, this, &QMainWindow::close);
}

void MailboxWidgets::insertActions()
{
    connect(ui->sendBtn, &QPushButton::clicked, [ = ]()
    {
        QString val = ui->textEdit->toPlainText();
        const char* msg = diplomat->sendf(addr, val);
        if (!msg)
        {
            ui->textEdit->clear();
            pushMsg(val);
        }
        else
        {
            ui->textEdit->setPlainText(msg);
        }
    });
    connect(ui->sendfBtn, &QPushButton::clicked, [ = ]()
    {
        QString fileName = QFileDialog::getOpenFileName(this,
                           tr("选择文件"),
                           "/",
                           tr("all (*.*)")
                                                       );
    if (fileName.isEmpty())return;
    fileName.insert(0, '\b');
    ui->textEdit->setPlainText(fileName);
    });
}

void MailboxWidgets::findMsg()
{
    msgs = NULL;
    Msgbox* idx = &(diplomat->firstbox);
    do
    {
        if (idx->addr == addr)
        {
            msgs = idx->list.load();
            break;
        }
        idx = idx->next;
    }
    while (idx);
    //找到消息头后把定时器重新连接到更新消息方法
    if (msgs)
    {
        QString msg(QString::fromUtf8(msgs->data));
        pushMsg(msg, false);
        disconnect(findMsgConn);
        connect(msgTimer, &QTimer::timeout, this, &MailboxWidgets::updateMsg);
    }
}

void MailboxWidgets::updateMsg()
{
    Msg* next;
    if ((next = msgs->next.load()) != NULL)
    {
        QString text(QString::fromUtf8(next->data));
        pushMsg(text, false);
        msgs = next;
    }
}
