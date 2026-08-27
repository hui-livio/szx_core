#pragma once

#include "../UIToolkitsExport.h"
#include <QComboBox>

namespace UIToolkits
{
class UITOOLKITS_API UTComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit UTComboBox(QWidget *parent = nullptr);
};
} // namespace UIToolkits
