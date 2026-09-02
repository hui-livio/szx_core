#pragma once
#include <QTimeEdit>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTTimePicker : public QTimeEdit
{
Q_OBJECT public : explicit UTTimePicker(QWidget *parent = nullptr);
};
} // namespace UIToolkits
