#pragma once

#include <QWidget>
#include "ui_msgwidget.h"
#include"diplomat.h"
#include<atomic>

QT_BEGIN_NAMESPACE
namespace Ui { class MsgWidgetClass; };
QT_END_NAMESPACE

class MsgWidget : public QWidget
{
	Q_OBJECT

public:
	MsgWidget(QString& msg, Diplomat* diplomat,unsigned long ip,QWidget *parent = nullptr,bool right = true);
	~MsgWidget();

private:
	QMetaObject::Connection loadingConn;
	Ui::MsgWidgetClass *ui;

	std::atomic_int proccess = -1;
	Diplomat *diplomat;

	void fitType(QString& msg,unsigned long ip);
	void fitPos(bool right);
};
