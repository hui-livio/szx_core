#pragma once

#include <QDialog>
#include "../UIToolkitsExport.h"

namespace UIToolkits
{
class UITOOLKITS_API UTDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UTDialog(QWidget *parent = nullptr);
};
} // namespace UIToolkits
