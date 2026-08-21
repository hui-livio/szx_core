/*
 * @file UTAppWindow.qml
 * @brief UIToolkits 的 UTAppWindow.qml 组件。
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    function syncThemeFont() {
        if (fontMedium.status !== FontLoader.Ready || fontMedium.name.length === 0)
            return;
        if (UTTheme.fontFamily !== fontMedium.name) {
            UTTheme.fontFamily = fontMedium.name;
            UTLogger.info("Theme font loaded: " + fontMedium.name, "UIToolkits.Theme");
        }
    }

    // ── Font ─────────────────────────────────────────────────────────
    /// 鸿蒙字体 (HarmonyOS Sans SC) — 全局字体，通过 Qt 资源系统加载
    FontLoader {
        id: fontRegular
        source: "qrc:/qt/qml/UIToolkits/fonts/HarmonyOS_Sans_SC_Regular.ttf"
    }
    FontLoader {
        id: fontMedium
        source: "qrc:/qt/qml/UIToolkits/fonts/HarmonyOS_Sans_SC_Medium.ttf"
        onStatusChanged: {
            if (status === FontLoader.Ready) {
                window.syncThemeFont();
            } else if (status === FontLoader.Error) {
                UTLogger.warn("Theme font failed to load", "UIToolkits.Theme");
            }
        }
        onNameChanged: window.syncThemeFont()
    }
    FontLoader {
        id: fontBold
        source: "qrc:/qt/qml/UIToolkits/fonts/HarmonyOS_Sans_SC_Bold.ttf"
    }

    /// 全局默认字体：鸿蒙 Medium，所有子控件自动继承
    font.family: fontMedium.status === FontLoader.Ready ? fontMedium.name : ""

    Component.onCompleted: syncThemeFont()

    // ── Theme ───────────────────────────────────────────────────────
    readonly property var theme: UTTheme.theme
    readonly property color borderColor: UTTheme.borderColor

    function nextTheme() {
        UTTheme.nextTheme();
    }
    function setTheme(k) {
        UTTheme.setTheme(k);
    }
    function toggleTheme() {
        UTTheme.toggleTheme();
    }
    function registerTheme(k, p) {
        UTTheme.registerTheme(k, p);
    }
    readonly property int contentMargin: 1

    // ── Layout ──────────────────────────────────────────────────────
    // 窗口底色作边框色，contentMargin 让内容区与窗口边缘保持间距形成边框
    color: borderColor
}
