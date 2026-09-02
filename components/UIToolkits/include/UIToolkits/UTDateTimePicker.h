#pragma once
#include <QDateTimeEdit>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTDateTimePicker : public QDateTimeEdit
{
Q_OBJECT public : explicit UTDateTimePicker(QWidget *parent = nullptr);
};
} // namespace UIToolkits
