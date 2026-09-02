#pragma once
#include <QTreeView>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTTreeView : public QTreeView
{
Q_OBJECT public : explicit UTTreeView(QWidget *parent = nullptr);
};
} // namespace UIToolkits
