#pragma once
#include <QPushButton>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTTabButton : public QPushButton
{
Q_OBJECT public : explicit UTTabButton(QWidget *parent = nullptr);
public slots:
    void setActive(bool active);
    bool isActive() const;
};
} // namespace UIToolkits
