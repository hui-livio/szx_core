#pragma once

#include <QLabel>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTLabel : public QLabel
{
    Q_OBJECT
public:
    explicit UTLabel(QWidget *parent = nullptr);
};
} // namespace UIToolkits
