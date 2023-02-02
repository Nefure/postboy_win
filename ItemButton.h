#pragma once
#include<QEvent>
#include<QToolButton>
#include <QWidget>
class ItemButton:public QToolButton
{

	unsigned long tag;

public:
	explicit ItemButton(QWidget* parent);

	void setTag(unsigned long newTag)
	{
		tag = newTag;
	}
	unsigned long getTag()
	{
		return tag;
	}

	void copyFrom(QToolButton* btn)
	{
		setIcon(btn->icon());
		setIconSize(btn->iconSize());
		setFocusPolicy(btn->focusPolicy());
		setAutoRaise(btn->autoRaise());
		setToolButtonStyle(btn->toolButtonStyle());
		setSizePolicy(btn->sizePolicy()); 
		setMinimumSize(btn->minimumSize());
	}

protected:

};

