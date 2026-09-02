#pragma once

#include <QComboBox>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit UTComboBox(QWidget *parent = nullptr);
};
} // namespace UIToolkits
