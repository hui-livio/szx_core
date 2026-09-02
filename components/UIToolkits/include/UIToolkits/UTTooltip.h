#pragma once
#include <QLabel>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTTooltip : public QLabel
{
Q_OBJECT public : explicit UTTooltip(QWidget *parent = nullptr);
};
} // namespace UIToolkits
