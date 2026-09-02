#pragma once
#include <QScrollArea>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTScrollView : public QScrollArea
{
Q_OBJECT public : explicit UTScrollView(QWidget *parent = nullptr);
};
} // namespace UIToolkits
