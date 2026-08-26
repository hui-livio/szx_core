/**
 * @file UTLineEdit.h
 * @brief UIToolkits QWidget line edit with the shared toolkit theme.
 */

#pragma once

#include "../UIToolkitsExport.h"

#include <QLineEdit>

namespace UIToolkits
{
class UITOOLKITS_API UTLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit UTLineEdit(QWidget *parent = nullptr);
};
} // namespace UIToolkits
