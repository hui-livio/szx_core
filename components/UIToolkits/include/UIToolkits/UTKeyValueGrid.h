#pragma once
#include "../UIToolkitsExport.h"
#include <QWidget>
namespace UIToolkits { class UITOOLKITS_API UTKeyValueGrid : public QWidget { Q_OBJECT public: explicit UTKeyValueGrid(QWidget *parent = nullptr); void addRow(const QString &key, const QString &value); void clearRows(); }; }
