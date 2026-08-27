#pragma once
#include "../UIToolkitsExport.h"
#include <QSlider>
namespace UIToolkits { class UITOOLKITS_API UTSliderValue : public QSlider { Q_OBJECT public: explicit UTSliderValue(Qt::Orientation orientation = Qt::Horizontal, QWidget *parent = nullptr); }; }
