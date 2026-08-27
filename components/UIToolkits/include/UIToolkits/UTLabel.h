#pragma once

#include "../UIToolkitsExport.h"
#include <QLabel>

namespace UIToolkits
{
class UITOOLKITS_API UTLabel : public QLabel
{
    Q_OBJECT
public:
    explicit UTLabel(QWidget *parent = nullptr);
};
} // namespace UIToolkits
