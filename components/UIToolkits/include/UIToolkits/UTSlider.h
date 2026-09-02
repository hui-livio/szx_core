#pragma once

#include <QSlider>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTSlider : public QSlider
{
    Q_OBJECT
public:
    explicit UTSlider(Qt::Orientation orientation = Qt::Horizontal, QWidget *parent = nullptr);
};
} // namespace UIToolkits
