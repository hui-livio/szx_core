/**
 * @file UTButton.cc
 * @brief Implementation of the QWidget button.
 */

#include "UIToolkits/UTButton.h"

namespace UIToolkits
{
UTButton::UTButton(QWidget *parent) :
    QPushButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(36);
    setObjectName(QStringLiteral("UTButton"));
}
} // namespace UIToolkits
