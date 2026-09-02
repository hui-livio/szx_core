#pragma once
#include <QPlainTextEdit>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTTextArea : public QPlainTextEdit
{
Q_OBJECT public : explicit UTTextArea(QWidget *parent = nullptr);
};
} // namespace UIToolkits
