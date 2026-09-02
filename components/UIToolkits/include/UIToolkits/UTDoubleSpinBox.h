#pragma once
#include <QDoubleSpinBox>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTDoubleSpinBox : public QDoubleSpinBox
{
Q_OBJECT public : explicit UTDoubleSpinBox(QWidget *parent = nullptr);
};
} // namespace UIToolkits
