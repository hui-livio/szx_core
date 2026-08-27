#pragma once
#include "../UIToolkitsExport.h"
#include <QMenu>
namespace UIToolkits { class UITOOLKITS_API UTMenu : public QMenu { Q_OBJECT public: explicit UTMenu(QWidget *parent = nullptr); explicit UTMenu(const QString &title, QWidget *parent = nullptr); }; }
