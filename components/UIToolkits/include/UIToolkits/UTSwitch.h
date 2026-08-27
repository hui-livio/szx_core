#pragma once

#include "../UIToolkitsExport.h"
#include <QCheckBox>

namespace UIToolkits
{
class UITOOLKITS_API UTSwitch : public QCheckBox
{
    Q_OBJECT
public:
    explicit UTSwitch(QWidget *parent = nullptr);
};
} // namespace UIToolkits
