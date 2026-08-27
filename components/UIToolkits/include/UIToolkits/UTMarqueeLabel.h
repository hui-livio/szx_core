#pragma once
#include "../UIToolkitsExport.h"
#include <QLabel>
namespace UIToolkits { class UITOOLKITS_API UTMarqueeLabel : public QLabel { Q_OBJECT public: explicit UTMarqueeLabel(QWidget *parent = nullptr); void setActive(bool active); bool isActive() const; void setSpeed(int pixelsPerTick); protected: void paintEvent(QPaintEvent *event) override; }; }
