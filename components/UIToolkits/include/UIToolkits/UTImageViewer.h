#pragma once
#include <QPixmap>
#include <QScrollArea>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTImageViewer : public QScrollArea
{
Q_OBJECT public : explicit UTImageViewer(QWidget *parent = nullptr);
    void setPixmap(const QPixmap &pixmap);
    QPixmap pixmap() const;
};
} // namespace UIToolkits
