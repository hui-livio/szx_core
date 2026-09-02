#pragma once

#include <QCheckBox>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTCheckBox : public QCheckBox
{
    Q_OBJECT
public:
    explicit UTCheckBox(QWidget *parent = nullptr);
};
} // namespace UIToolkits
