#pragma once
#include <QLabel>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTImageAvatar : public QLabel
{
Q_OBJECT public : explicit UTImageAvatar(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
} // namespace UIToolkits
