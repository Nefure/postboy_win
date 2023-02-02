#include "msgWidget.h"

#include<time.h>
#include<qfiledialog.h>
#include<qtimer.h>

static QTimer* timer = NULL;

MsgWidget::MsgWidget(QString& msg, Diplomat* _diplomat, unsigned long ip, QWidget* parent, bool right)
    : QWidget(parent)
    , ui(new Ui::MsgWidgetClass())
    , diplomat(_diplomat)
{

    if (timer == NULL)
    {
        timer = new QTimer(parent);
        timer->start(1000);
    }

    ui->setupUi(this);

    fitPos(right);

    fitType(msg, ip);

    time_t timep;
    struct tm* p;

    time(&timep); //获取从1970至今过了多少秒，存入time_t类型的timep
    p = localtime(&timep);
    QString time(QString::number(p->tm_hour));
    time.append(" : ");
    time.append(QString::number(p->tm_min));
    ui->label->setText(time);
    ui->textlab->setText(msg);
}

MsgWidget::~MsgWidget()
{
    delete ui;
}

void MsgWidget::fitType(QString& msg, unsigned long ip)
{
    if (msg.startsWith('\b'))
    {
        msg.remove(0, 1);
        msg.append("  ");
        ui->textlab->hide();
        connect(ui->requestButton, &QToolButton::clicked, [ = ]
        {
            if (this->proccess.load() >= 0)
            {
                return;
            }
            QString path = QFileDialog::getExistingDirectory(this, "请选择要下载到哪个目录", "/");
            if(!path.isEmpty())
            diplomat->requestf(ip, msg, path, &this->proccess);
        });
        msg.remove(0, msg.lastIndexOf('/') +1);
            loadingConn = connect(timer, &QTimer::timeout, [ = ]
            {
                int loaded = this->proccess.load();
            if (loaded == -4)
            {
                ui->requestButton->setText(msg + "(下载完成)");
            }
            else if (loaded == -3)
            {
                ui->requestButton->setText(msg + "...(文件下载完成，但重命名失败，请手动删除后缀)");
            }
               else if (loaded == -2)
                {
                    ui->requestButton->setText("下载失败");
                }
                else if (loaded == -1)
                {
                    ui->requestButton->setText(msg);
                }
                else if (loaded == 100)
                {
                    ui->requestButton->setText(msg + "(下载完成，正在修改文件名..)");
                }
                else
                {
                    QString txt = QString::number(loaded);
                    txt.append("%");
                    ui->requestButton->setText(msg + txt);
                }
            });
        ui->requestButton->setText(msg);
    }
    else
    {
        ui->requestButton->hide();
    }
}

void MsgWidget::fitPos(bool right)
{
    if (right)
    {
        ui->bodylayout->removeItem(ui->rightSpacer);
        ui->textlayout->removeItem(ui->inrightSpacer);
        ui->leftWidget->hide();
        ui->textlab->setStyleSheet("color:white;background-color:rgb(70,69,69);");
    }
    else
    {
        ui->bodylayout->removeItem(ui->leftSpacer);
        ui->textlayout->removeItem(ui->inleftSpacer);
        ui->leftsymbol->hide();
    }
    ui->textlab->adjustSize();
}
