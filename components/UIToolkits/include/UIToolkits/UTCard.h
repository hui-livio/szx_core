#pragma once

#include "../UIToolkitsExport.h"
#include <QFrame>

namespace UIToolkits
{
class UITOOLKITS_API UTCard : public QFrame
{
    Q_OBJECT
public:
    explicit UTCard(QWidget *parent = nullptr);
};
} // namespace UIToolkits
