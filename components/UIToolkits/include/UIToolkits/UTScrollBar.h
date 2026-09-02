#pragma once
#include <QScrollBar>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTScrollBar : public QScrollBar
{
Q_OBJECT public : explicit UTScrollBar(Qt::Orientation orientation = Qt::Vertical, QWidget *parent = nullptr);
};
} // namespace UIToolkits
