#pragma once
#include "../UIToolkitsExport.h"
#include <QMainWindow>
namespace UIToolkits { class UITOOLKITS_API UTAppWindow : public QMainWindow { Q_OBJECT public: explicit UTAppWindow(QWidget *parent = nullptr); void setFrameless(bool enabled); }; }
