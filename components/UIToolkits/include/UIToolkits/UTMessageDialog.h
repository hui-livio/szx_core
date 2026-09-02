#pragma once
#include <QDialog>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTMessageDialog : public QDialog
{
Q_OBJECT public : explicit UTMessageDialog(QWidget *parent = nullptr);
    void setMessage(const QString &message);
    QString message() const;
};
} // namespace UIToolkits
