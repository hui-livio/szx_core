#pragma once
#include <QToolButton>
#include <QWidget>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTTitleBarButtonGroup : public QWidget
{
Q_OBJECT public : explicit UTTitleBarButtonGroup(QWidget *parent = nullptr);
    QToolButton *addButton(const QString &text);
};
} // namespace UIToolkits
