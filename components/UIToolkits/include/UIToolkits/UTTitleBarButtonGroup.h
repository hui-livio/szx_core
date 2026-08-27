#pragma once
#include "../UIToolkitsExport.h"
#include <QWidget>
#include <QToolButton>
namespace UIToolkits { class UITOOLKITS_API UTTitleBarButtonGroup : public QWidget { Q_OBJECT public: explicit UTTitleBarButtonGroup(QWidget *parent = nullptr); QToolButton *addButton(const QString &text); }; }
