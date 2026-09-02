#pragma once

#include <QCheckBox>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTSwitch : public QCheckBox
{
    Q_OBJECT
public:
    explicit UTSwitch(QWidget *parent = nullptr);
};
} // namespace UIToolkits
