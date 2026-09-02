#pragma once
#include <QSlider>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTSliderValue : public QSlider
{
Q_OBJECT public : explicit UTSliderValue(Qt::Orientation orientation = Qt::Horizontal, QWidget *parent = nullptr);
    QString valueText() const;
};
} // namespace UIToolkits
