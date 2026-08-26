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
    setObjectName(QStringLiteral("UTLineEdit"));
}
} // namespace UIToolkits
