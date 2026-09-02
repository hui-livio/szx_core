#pragma once

#include <QFrame>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTCard : public QFrame
{
    Q_OBJECT
public:
    explicit UTCard(QWidget *parent = nullptr);
};
} // namespace UIToolkits
