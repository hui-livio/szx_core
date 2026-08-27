#pragma once
#include "../UIToolkitsExport.h"
#include <QProgressBar>
namespace UIToolkits { class UITOOLKITS_API UTLoading : public QProgressBar { Q_OBJECT public: explicit UTLoading(QWidget *parent = nullptr); }; }
