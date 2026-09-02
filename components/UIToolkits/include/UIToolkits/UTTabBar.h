#pragma once
#include <QTabBar>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTTabBar : public QTabBar
{
Q_OBJECT public : explicit UTTabBar(QWidget *parent = nullptr);
};
} // namespace UIToolkits
