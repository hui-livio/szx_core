/*
 * @file DemoTabBar.qml
 * @brief UIToolkits 展示应用的 DemoTabBar.qml 页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null

    readonly property var theme: UTTheme.theme

    property string currentTab: ""

    contentWidth: contentCol.width
    contentHeight: contentCol.implicitHeight

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 16

        UTLabel {
            text: "Tab Bar"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "UTTabBar — 主题化标签栏，支持指示器样式自定义"
            textRole: UTLabel.TextRole.Caption
            color: theme.secondaryTextColor
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        // ── 基础 TabBar ──────────────────────────────────────────
        UTLabel {
            text: "基础样式"
            textRole: UTLabel.TextRole.H4
            color: theme.primaryTextColor
        }

        UTTabBar {
            id: basicTabs
            Layout.fillWidth: true

            UTTabButton {
                text: "检测配置"
            }
            UTTabButton {
                text: "缺陷管理"
            }
            UTTabButton {
                text: "报告导出"
            }
            UTTabButton {
                text: "系统设置"
            }
        }

        StackLayout {
            id: basicStack
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            currentIndex: basicTabs.currentIndex

            Rectangle {
                color: theme.surfaceColor
                radius: theme.radius || 8
                border.width: 1
                border.color: theme.borderColor
                UTLabel {
                    anchors.centerIn: parent
                    text: "检测配置页面内容"
                    textRole: UTLabel.TextRole.Body
                    color: theme.secondaryTextColor
                }
            }
            Rectangle {
                color: theme.surfaceColor
                radius: theme.radius || 8
                border.width: 1
                border.color: theme.borderColor
                UTLabel {
                    anchors.centerIn: parent
                    text: "缺陷管理页面内容"
                    textRole: UTLabel.TextRole.Body
                    color: theme.secondaryTextColor
                }
            }
            Rectangle {
                color: theme.surfaceColor
                radius: theme.radius || 8
                border.width: 1
                border.color: theme.borderColor
                UTLabel {
                    anchors.centerIn: parent
                    text: "报告导出页面内容"
                    textRole: UTLabel.TextRole.Body
                    color: theme.secondaryTextColor
                }
            }
            Rectangle {
                color: theme.surfaceColor
                radius: theme.radius || 8
                border.width: 1
                border.color: theme.borderColor
                UTLabel {
                    anchors.centerIn: parent
                    text: "系统设置页面内容"
                    textRole: UTLabel.TextRole.Body
                    color: theme.secondaryTextColor
                }
            }
        }

        // ── 禁用 Tab ─────────────────────────────────────────────
        UTLabel {
            text: "含禁用项"
            textRole: UTLabel.TextRole.H4
            color: theme.primaryTextColor
        }

        UTTabBar {
            id: disabledTabs
            Layout.fillWidth: true

            UTTabButton {
                text: "基本信息"
            }
            UTTabButton {
                text: "高级设置"
                enabled: false
            }
            UTTabButton {
                text: "日志管理"
            }
        }

        UTLabel {
            text: "当前: " + (disabledTabs.currentIndex >= 0 ? ["基本信息", "高级设置", "日志管理"][disabledTabs.currentIndex] : "—")
            textRole: UTLabel.TextRole.Caption
            color: theme.primaryColor
        }

        // ── 带图标 Tab ───────────────────────────────────────────
        UTLabel {
            text: "带图标"
            textRole: UTLabel.TextRole.H4
            color: theme.primaryTextColor
        }

        UTTabBar {
            id: iconTabs
            Layout.fillWidth: true

            UTTabButton {
                text: "📊 图表"
            }
            UTTabButton {
                text: "📋 列表"
            }
            UTTabButton {
                text: "⚙️ 配置"
            }
            UTTabButton {
                text: "📄 报告"
            }
        }

        UTLabel {
            text: "选中: " + (iconTabs.currentIndex >= 0 ? ["图表", "列表", "配置", "报告"][iconTabs.currentIndex] : "—")
            textRole: UTLabel.TextRole.Caption
            color: theme.primaryColor
        }

        Item {
            Layout.preferredHeight: 8
        }
    }
}
