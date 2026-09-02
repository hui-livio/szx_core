#include "UIToolkits/UTDatePicker.h"
#include "UIToolkits/UTDateTimePicker.h"
#include "UIToolkits/UTDoubleSpinBox.h"
#include "UIToolkits/UTGroupBox.h"
#include "UIToolkits/UTImage.h"
#include "UIToolkits/UTListView.h"
#include "UIToolkits/UTMenu.h"
#include "UIToolkits/UTMenuBar.h"
#include "UIToolkits/UTRoundButton.h"
#include "UIToolkits/UTScrollBar.h"
#include "UIToolkits/UTSvgButton.h"
#include "UIToolkits/UTTabBar.h"
#include "UIToolkits/UTTextArea.h"
#include "UIToolkits/UTTimePicker.h"
#include "UIToolkits/UTTreeView.h"

namespace UIToolkits
{
UTDoubleSpinBox::UTDoubleSpinBox(QWidget *parent) :
    QDoubleSpinBox(parent)
{
}
UTTextArea::UTTextArea(QWidget *parent) :
    QPlainTextEdit(parent)
{
}
UTRoundButton::UTRoundButton(QWidget *parent) :
    QPushButton(parent)
{
}
UTSvgButton::UTSvgButton(QWidget *parent) :
    QPushButton(parent)
{
}
UTTabBar::UTTabBar(QWidget *parent) :
    QTabBar(parent)
{
}
UTScrollBar::UTScrollBar(Qt::Orientation orientation, QWidget *parent) :
    QScrollBar(orientation, parent)
{
}
UTGroupBox::UTGroupBox(QWidget *parent) :
    QGroupBox(parent)
{
}
UTListView::UTListView(QWidget *parent) :
    QListView(parent)
{
}
UTTreeView::UTTreeView(QWidget *parent) :
    QTreeView(parent)
{
}
UTDatePicker::UTDatePicker(QWidget *parent) :
    QDateEdit(parent)
{
}
UTTimePicker::UTTimePicker(QWidget *parent) :
    QTimeEdit(parent)
{
}
UTDateTimePicker::UTDateTimePicker(QWidget *parent) :
    QDateTimeEdit(parent)
{
}
UTMenu::UTMenu(QWidget *parent) :
    QMenu(parent)
{
}
UTMenu::UTMenu(const QString &title, QWidget *parent) :
    QMenu(title, parent)
{
}
UTMenuBar::UTMenuBar(QWidget *parent) :
    QMenuBar(parent)
{
}
UTImage::UTImage(QWidget *parent) :
    QLabel(parent)
{
}
} // namespace UIToolkits
