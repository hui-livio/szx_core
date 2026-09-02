#pragma once
#include <QWidget>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTTitleBar : public QWidget
{
Q_OBJECT public : explicit UTTitleBar(QWidget *parent = nullptr);
    void setTitle(const QString &title);
    QString title() const;
signals:
    void closeRequested();
    void minimizeRequested();
    void maximizeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
};
} // namespace UIToolkits
