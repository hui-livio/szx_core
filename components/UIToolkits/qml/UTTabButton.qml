/*
 * @file UTTabButton.qml
 * @brief 浏览器风格标签按钮 — Edge/Chrome 风格圆角顶部、悬浮高亮、底部连通感
 */

// ========================================================================
// ========================================================================

import QtQuick
import QtQuick.Controls.Basic

TabButton {
    id: root

    // ── 外观 ──────────────────────────────────────────────────────

    property color accentColor: UTTheme.primaryColor
    property color activeBg: UTTheme.surfaceColor
    property color hoverBg: {
        let c = accentColor;
        return Qt.rgba(c.r, c.g, c.b, 0.07);
    }
    property color textActive: UTTheme.primaryColor
    property color textInactive: UTTheme.secondaryTextColor
    property int radius: 6

    implicitHeight: 38
    leftPadding: 16
    rightPadding: 16
    topPadding: 0
    bottomPadding: 0

    font {
        pixelSize: 13
    }

    // ── 按压缩放反馈 ──────────────────────────────────────────────
    scale: pressed ? 0.97 : 1.0
    Behavior on scale {
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutBack
        }
    }

    // ── 内容 ──────────────────────────────────────────────────────
    contentItem: Text {
        text: root.text
        font.family: root.font.family
        font.pixelSize: root.font.pixelSize
        font.weight: root.checked ? Font.DemiBold : Font.Normal
        color: root.checked ? root.textActive : (root.hovered ? Qt.lighter(root.textInactive, 1.4) : root.textInactive)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        scale: root.checked ? 1.04 : 1.0

        Behavior on color {
            ColorAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
        Behavior on scale {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }

    // ── 背景 — 浏览器标签形状 ─────────────────────────────────────
    background: Rectangle {
        id: bg
        // 顶部圆角，底部直角 — 形成"连接内容区"的视觉效果
        radius: root.radius
        color: {
            if (root.checked)
                return root.activeBg;
            if (root.hovered)
                return root.hoverBg;
            return "transparent";
        }

        // 底部直角覆盖 — 仅激活态形成"连通内容区"视觉效果
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: root.radius
            color: root.checked ? bg.color : "transparent"
        }
    }

    // ── 手势 ──────────────────────────────────────────────────────
    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
