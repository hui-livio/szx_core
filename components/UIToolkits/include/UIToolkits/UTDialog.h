#pragma once

#include "../UIToolkitsExport.h"
#include <QDialog>

namespace UIToolkits
{
class UITOOLKITS_API UTDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UTDialog(QWidget *parent = nullptr);
};
} // namespace UIToolkits
