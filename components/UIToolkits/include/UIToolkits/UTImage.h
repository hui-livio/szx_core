#pragma once
#include <QLabel>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTImage : public QLabel
{
Q_OBJECT public : explicit UTImage(QWidget *parent = nullptr);
};
} // namespace UIToolkits
