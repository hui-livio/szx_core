#pragma once
#include <QLabel>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTMarqueeLabel : public QLabel
{
Q_OBJECT public : explicit UTMarqueeLabel(QWidget *parent = nullptr);
    void setActive(bool active);
    bool isActive() const;
    void setSpeed(int pixelsPerTick);

protected:
    void paintEvent(QPaintEvent *event) override;
};
} // namespace UIToolkits
