#pragma once

#include <QSpinBox>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    explicit UTSpinBox(QWidget *parent = nullptr);
};
} // namespace UIToolkits
