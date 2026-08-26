/**
 * @file UTWidgetTheme.h
 * @brief QWidget theme adapter backed by the UIToolkits theme tokens.
 */

#pragma once

#include "../UIToolkitsExport.h"

#include <QPalette>

class QApplication;

namespace UIToolkits
{
enum class UTWidgetThemeKind
{
    Light,
    Dark,
    Warm,
    Forest,
};

class UITOOLKITS_API UTWidgetTheme final
{
public:
    static QPalette palette(UTWidgetThemeKind kind);
    static void apply(QApplication &application, UTWidgetThemeKind kind);
};
} // namespace UIToolkits
