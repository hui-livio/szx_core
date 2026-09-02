#pragma once
#include <QScrollArea>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTFlickable : public QScrollArea
{
Q_OBJECT public : explicit UTFlickable(QWidget *parent = nullptr);
};
} // namespace UIToolkits
