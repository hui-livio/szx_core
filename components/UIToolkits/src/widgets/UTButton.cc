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
    setStyleSheet(QStringLiteral(
        "QPushButton { background: #2f80ed; color: white; border: none; "
        "border-radius: 6px; padding: 6px 16px; }"
        "QPushButton:hover { background: #256dcc; }"
        "QPushButton:pressed { background: #1d5aa7; }"
        "QPushButton:disabled { background: #b7c2d0; color: #eef2f6; }"));
}
} // namespace UIToolkits
