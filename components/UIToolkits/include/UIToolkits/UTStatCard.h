#pragma once
#include "../UIToolkitsExport.h"
#include <QFrame>
namespace UIToolkits { class UITOOLKITS_API UTStatCard : public QFrame { Q_OBJECT public: explicit UTStatCard(QWidget *parent = nullptr); void setTitle(const QString &title); void setValue(const QString &value); QString title() const; QString value() const; }; }
