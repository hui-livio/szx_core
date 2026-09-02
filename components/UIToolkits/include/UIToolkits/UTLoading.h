#pragma once
#include <QProgressBar>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTLoading : public QProgressBar
{
Q_OBJECT public : explicit UTLoading(QWidget *parent = nullptr);
public slots:
    void start();
    void stop();
    bool isActive() const;
};
} // namespace UIToolkits
