#pragma once
#include <QCalendarWidget>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTCalendar : public QCalendarWidget
{
Q_OBJECT public : explicit UTCalendar(QWidget *parent = nullptr);
};
} // namespace UIToolkits
