#pragma once
#include <QMainWindow>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTAppWindow : public QMainWindow
{
Q_OBJECT public : explicit UTAppWindow(QWidget *parent = nullptr);
    void setFrameless(bool enabled);
};
} // namespace UIToolkits
