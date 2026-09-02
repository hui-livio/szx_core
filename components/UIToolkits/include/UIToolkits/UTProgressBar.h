#pragma once

#include <QProgressBar>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTProgressBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit UTProgressBar(QWidget *parent = nullptr);
};
} // namespace UIToolkits
