/*
 * @file UTStatCard.qml
 * @brief 统计卡片 — 上下结构，上方数值 + 下方标题
 */

// ========================================================================
// ========================================================================

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: root

    // ── Theme ────────────────────────────────────────────────────

    // ── Content ──────────────────────────────────────────────────
    // 数值
    property alias value: valueLabel.text
    // 标题
    property alias title: titleLabel.text

    // ── Style ────────────────────────────────────────────────────
    // 主题色覆盖 (空则使用全局主题色)
    property color accentColor: UTTheme.primaryColor
    // 圆角
    property real cardRadius: UTTheme.radius || 8

    // ── 无障碍 ──────────────────────────────────────────────────
    property string accName: title + " " + value
    property int accRole: Accessible.StaticText
    Accessible.name: accName
    Accessible.role: accRole

    implicitWidth: 100
    implicitHeight: 80
    radius: cardRadius
    color: "transparent"

    // ── 背景 ────────────────────────────────────────────────────
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.cardRadius
        color: UTTheme.surfaceColor
        border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.2)
        border.width: 1
    }

    // ── 阴影 ────────────────────────────────────────────────────
    MultiEffect {
        source: bg
        anchors.fill: bg
        shadowEnabled: true
        shadowColor: UTTheme.shadowColor
        shadowBlur: 6
        shadowVerticalOffset: 2
        shadowHorizontalOffset: 0
    }

    // ── 内容布局 ────────────────────────────────────────────────
    ColumnLayout {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        // 标题
        UTLabel {
            id: titleLabel
            // textRole: UTLabel.TextRole.Caption
            color: UTTheme.secondaryTextColor
        }
        // 数值
        UTLabel {
            id: valueLabel
            textRole: UTLabel.TextRole.H2
            color: root.accentColor
            font.weight: Font.Bold
        }
    }
}
