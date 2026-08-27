#pragma once

#include "../UIToolkitsExport.h"
#include <QProgressBar>

namespace UIToolkits
{
class UITOOLKITS_API UTProgressBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit UTProgressBar(QWidget *parent = nullptr);
};
} // namespace UIToolkits
