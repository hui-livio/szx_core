/**
 * @file UTWidgetTheme.cc
 * @brief QWidget theme adapter implementation.
 */

#include "UIToolkits/UTWidgetTheme.h"

#include <QApplication>

namespace
{
void setCommonPalette(QPalette &palette, const QColor &window, const QColor &surface, const QColor &text,
                      const QColor &secondaryText, const QColor &border, const QColor &primary)
{
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::Base, surface);
    palette.setColor(QPalette::AlternateBase, window);
    palette.setColor(QPalette::Button, surface);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::PlaceholderText, secondaryText);
    palette.setColor(QPalette::Mid, border);
    palette.setColor(QPalette::Highlight, primary);
    palette.setColor(QPalette::HighlightedText, QColor(Qt::white));
}
} // namespace

namespace UIToolkits
{
QPalette UTWidgetTheme::palette(UTWidgetThemeKind kind)
{
    QPalette result;
    switch (kind)
    {
    case UTWidgetThemeKind::Light:
        setCommonPalette(result, QColor(QStringLiteral("#f5f7fa")), QColor(QStringLiteral("#ffffff")),
                         QColor(QStringLiteral("#181d27")), QColor(QStringLiteral("#5a6577")),
                         QColor(QStringLiteral("#d5dbe3")), QColor(QStringLiteral("#3a6df0")));
        break;
    case UTWidgetThemeKind::Warm:
        setCommonPalette(result, QColor(QStringLiteral("#fcf9f5")), QColor(QStringLiteral("#f5ede3")),
                         QColor(QStringLiteral("#231c16")), QColor(QStringLiteral("#6b5543")),
                         QColor(QStringLiteral("#dfd3c5")), QColor(QStringLiteral("#e0732f")));
        break;
    case UTWidgetThemeKind::Forest:
        setCommonPalette(result, QColor(QStringLiteral("#f2f7f3")), QColor(QStringLiteral("#e3ede6")),
                         QColor(QStringLiteral("#17241c")), QColor(QStringLiteral("#4d6654")),
                         QColor(QStringLiteral("#ced9d0")), QColor(QStringLiteral("#2e9e5b")));
        break;
    case UTWidgetThemeKind::Dark:
    default:
        setCommonPalette(result, QColor(QStringLiteral("#0f1217")), QColor(QStringLiteral("#1a1e25")),
                         QColor(QStringLiteral("#e6edf5")), QColor(QStringLiteral("#8b949e")),
                         QColor(QStringLiteral("#404854")), QColor(QStringLiteral("#6d9fff")));
        result.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#6b7280")));
        result.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#6b7280")));
        break;
    }
    return result;
}

void UTWidgetTheme::apply(QApplication &application, UTWidgetThemeKind kind)
{
    application.setPalette(palette(kind));
    application.setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background: palette(window); color: palette(window-text); }"
        "QListWidget { background: palette(base); border: 1px solid palette(mid); }"
        "QListWidget::item { padding: 8px; }"
        "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
        "QPushButton#UTButton { background: palette(highlight); color: palette(highlighted-text); "
        "border: none; border-radius: 6px; padding: 6px 16px; min-height: 24px; }"
        "QPushButton#UTButton:hover { background: palette(highlight); }"
        "QPushButton#UTButton:disabled { background: palette(mid); color: palette(disabled, text); }"
        "QLineEdit#UTLineEdit { background: palette(base); color: palette(text); border: 1px solid palette(mid); "
        "border-radius: 6px; padding: 6px 10px; min-height: 22px; }"
        "QLineEdit#UTLineEdit:focus { border: 2px solid palette(highlight); padding: 5px 9px; }"
        "QGroupBox { border: 1px solid palette(mid); border-radius: 8px; margin-top: 12px; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"));
}
} // namespace UIToolkits
