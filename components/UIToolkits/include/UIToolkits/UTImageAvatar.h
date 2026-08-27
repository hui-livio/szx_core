#pragma once
#include "../UIToolkitsExport.h"
#include <QLabel>
namespace UIToolkits { class UITOOLKITS_API UTImageAvatar : public QLabel { Q_OBJECT public: explicit UTImageAvatar(QWidget *parent = nullptr); protected: void paintEvent(QPaintEvent *event) override; }; }
