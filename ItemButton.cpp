#include "ItemButton.h"

ItemButton::ItemButton(QWidget* parent) :QToolButton(parent), tag((unsigned long)0)
{
    setCursor(Qt::PointingHandCursor);
}
