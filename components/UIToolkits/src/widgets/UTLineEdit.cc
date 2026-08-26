/**
 * @file UTLineEdit.cc
 * @brief Implementation of the QWidget line edit.
 */

#include "UIToolkits/UTLineEdit.h"

namespace UIToolkits
{
UTLineEdit::UTLineEdit(QWidget *parent) :
    QLineEdit(parent)
{
    setMinimumHeight(36);
    setStyleSheet(QStringLiteral(
        "QLineEdit { background: white; color: #1f2937; border: 1px solid #cbd5e1; "
        "border-radius: 6px; padding: 6px 10px; }"
        "QLineEdit:focus { border: 2px solid #2f80ed; padding: 5px 9px; }"));
}
} // namespace UIToolkits
