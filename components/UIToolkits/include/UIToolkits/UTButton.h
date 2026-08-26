/**
 * @file UTButton.h
 * @brief UIToolkits QWidget button with the shared toolkit theme.
 */

#pragma once

#include "../UIToolkitsExport.h"

#include <QPushButton>

namespace UIToolkits
{
class UITOOLKITS_API UTButton : public QPushButton
{
    Q_OBJECT

public:
    explicit UTButton(QWidget *parent = nullptr);
};
} // namespace UIToolkits
