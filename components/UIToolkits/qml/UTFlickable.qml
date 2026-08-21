/*
 * @file UTFlickable.qml
 * @brief 可滚动容器 — 基于 Qt Flickable，封装主题化 ScrollBar 与按需显示
 */

// ========================================================================
// ========================================================================

import QtQuick
import QtQuick.Controls.Basic

Flickable {
    id: root

    // ── 内部主题（不与外部 theme 属性冲突）───────────────────────────
    readonly property color scrollbarTrackColor: UTTheme.scrollbarTrackColor
    readonly property color scrollbarThumbNormalColor: UTTheme.scrollbarThumbNormalColor
    readonly property color scrollbarThumbHoverColor: UTTheme.scrollbarThumbHoverColor
    readonly property color scrollbarThumbPressedColor: UTTheme.scrollbarThumbPressedColor

    // ── 预置滚动行为 ─────────────────────────────────────────────────
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickDeceleration: 2000
    maximumFlickVelocity: 4000

    // ── 按需显示策略 ─────────────────────────────────────────────────
    property bool showScrollBar: true
    property bool _needHBar: contentWidth > width
    property bool _needVBar: contentHeight > height

    // ── 主题化水平滚动条 ─────────────────────────────────────────────
    ScrollBar.horizontal: ScrollBar {
        id: hBar
        policy: root.showScrollBar && root._needHBar ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        contentItem: Rectangle {
            implicitHeight: 10
            radius: 5
            color: root.scrollbarTrackColor

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: parent.radius
                color: hBar.pressed ? root.scrollbarThumbPressedColor : (hBar.hovered ? root.scrollbarThumbHoverColor : root.scrollbarThumbNormalColor)

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }
            }
        }
    }

    // ── 主题化垂直滚动条 ─────────────────────────────────────────────
    ScrollBar.vertical: ScrollBar {
        id: vBar
        policy: root.showScrollBar && root._needVBar ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        contentItem: Rectangle {
            implicitWidth: 10
            radius: 5
            color: root.scrollbarTrackColor

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: parent.radius
                color: vBar.pressed ? root.scrollbarThumbPressedColor : (vBar.hovered ? root.scrollbarThumbHoverColor : root.scrollbarThumbNormalColor)

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }
            }
        }
    }
}
