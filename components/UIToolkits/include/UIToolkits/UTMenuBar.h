#pragma once
#include <QMenuBar>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTMenuBar : public QMenuBar
{
Q_OBJECT public : explicit UTMenuBar(QWidget *parent = nullptr);
};
} // namespace UIToolkits
