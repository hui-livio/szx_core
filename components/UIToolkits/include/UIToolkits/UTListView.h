#pragma once
#include <QListView>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTListView : public QListView
{
Q_OBJECT public : explicit UTListView(QWidget *parent = nullptr);
};
} // namespace UIToolkits
