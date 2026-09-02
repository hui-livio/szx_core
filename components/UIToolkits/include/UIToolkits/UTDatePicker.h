#pragma once
#include <QDateEdit>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTDatePicker : public QDateEdit
{
Q_OBJECT public : explicit UTDatePicker(QWidget *parent = nullptr);
};
} // namespace UIToolkits
