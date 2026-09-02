#pragma once
#include <QLabel>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTToast : public QLabel
{
Q_OBJECT public : explicit UTToast(QWidget *parent = nullptr);
public slots:
    void showMessage(const QString &message, int durationMs = 2500);
};
} // namespace UIToolkits
