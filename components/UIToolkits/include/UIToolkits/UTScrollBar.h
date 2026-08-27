#pragma once
#include "../UIToolkitsExport.h"
#include <QScrollBar>
namespace UIToolkits { class UITOOLKITS_API UTScrollBar : public QScrollBar { Q_OBJECT public: explicit UTScrollBar(Qt::Orientation orientation = Qt::Vertical, QWidget *parent = nullptr); }; }
