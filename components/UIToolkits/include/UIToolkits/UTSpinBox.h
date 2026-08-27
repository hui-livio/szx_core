#pragma once

#include "../UIToolkitsExport.h"
#include <QSpinBox>

namespace UIToolkits
{
class UITOOLKITS_API UTSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    explicit UTSpinBox(QWidget *parent = nullptr);
};
} // namespace UIToolkits
