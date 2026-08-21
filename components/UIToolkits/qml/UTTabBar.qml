/*
 * @file UTTabBar.qml
 * @brief 浏览器风格标签栏 — Edge/Chrome 风格，圆角顶部 + 底部连通感 + accent 色条
 */

// ========================================================================
// ========================================================================

import QtQuick
import QtQuick.Controls.Basic

TabBar {
    id: root

    // ── 共享配色（UTTabButton 通过 bar.xxx 引用）────────────────
    property color accentColor: UTTheme.primaryColor
    property color activeBg: UTTheme.surfaceColor
    property color hoverBg: {
        let c = UTTheme.primaryColor;
        return Qt.rgba(c.r, c.g, c.b, 0.07);
    }
    property color textActive: UTTheme.primaryTextColor
    property color textInactive: UTTheme.secondaryTextColor

    // ── 尺寸 ──────────────────────────────────────────────────────
    property int tabHeight: 38
    property int tabSpacing: 0

    // ── 无障碍 ────────────────────────────────────────────────────
    Accessible.name: "Tab Bar"
    Accessible.role: Accessible.PageTabList

    spacing: tabSpacing
    implicitHeight: tabHeight

    // ── 背景：底部分隔线 ───────────────────────────────────────
    background: Rectangle {
        color: "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: UTTheme.borderColor
        }
    }

    // ── 内容容器 ──────────────────────────────────────────────────
    contentItem: ListView {
        id: tabList
        model: root.contentModel
        orientation: ListView.Horizontal
        spacing: root.tabSpacing
        interactive: false
        implicitHeight: root.tabHeight
        leftMargin: 0
        rightMargin: 0
    }
}
