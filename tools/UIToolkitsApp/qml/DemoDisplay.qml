/*
 * @file DemoDisplay.qml
 * @brief UIToolkits 展示应用的 DemoDisplay.qml 页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null

    readonly property var theme: UTTheme.theme

    contentWidth: contentCol.width
    contentHeight: contentCol.implicitHeight

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 24

        UTLabel {
            text: "Display"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "Typography"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "Heading 1 - Large Title"
            textRole: UTLabel.TextRole.H1
        }

        UTLabel {
            text: "Heading 4 - Subheading"
            textRole: UTLabel.TextRole.H4
        }

        UTLabel {
            text: "Body text - Regular text content with proper line height"
            textRole: UTLabel.TextRole.Body
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        UTLabel {
            text: "Caption - Small supporting text"
            textRole: UTLabel.TextRole.Caption
            color: theme.secondaryTextColor
        }

        UTLabel {
            text: "Card"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "单击 / 双击测试"
            textRole: UTLabel.TextRole.Caption
            color: theme.secondaryTextColor
        }

        UTCard {
            title: "基础卡片"
            subtitle: "单击弹出 Info Toast"
            elevation: 1
            Layout.fillWidth: true

            onClicked: root.toast && root.toast.showToast("单击: 基础卡片", UTToast.ToastType.Info, 1200)
            onDoubleClicked: root.toast && root.toast.showToast("双击: 基础卡片", UTToast.ToastType.Success, 1500)
        }

        UTCard {
            title: "带头像卡片"
            subtitle: "双击弹出 Success Toast"
            avatarSize: 48
            elevation: 2
            Layout.fillWidth: true

            onClicked: root.toast && root.toast.showToast("单击: 带头像卡片", UTToast.ToastType.Info, 1200)
            onDoubleClicked: root.toast && root.toast.showToast("双击: 带头像卡片", UTToast.ToastType.Success, 1500)
        }

        UTCard {
            title: "选中状态卡片"
            subtitle: "selected = true"
            selected: true
            elevation: 1
            Layout.fillWidth: true

            onClicked: root.toast && root.toast.showToast("单击: 选中卡片", UTToast.ToastType.Warning, 1200)
            onDoubleClicked: root.toast && root.toast.showToast("双击: 选中卡片", UTToast.ToastType.Success, 1500)
        }

        UTLabel {
            text: "Avatar"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            UTImageAvatar {
                size: 38
                fallbackText: "UT"
            }
            UTImageAvatar {
                size: 52
                fallbackText: "JS"
                shape: UTImageAvatar.Shape.RoundedRect
            }
            UTImageAvatar {
                size: 64
                fallbackText: "A"
            }
        }

        UTLabel {
            text: "StatCard"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "数值 + 标题 统计卡片"
            textRole: UTLabel.TextRole.Caption
            color: theme.secondaryTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UTStatCard {
                title: "Total"
                value: "128"
                accentColor: "#3a6df0"
            }
            UTStatCard {
                title: "NG"
                value: "3"
                accentColor: "#e53935"
            }
            UTStatCard {
                title: "OK"
                value: "125"
                accentColor: "#43a047"
            }
            UTStatCard {
                title: "Correct"
                value: "0"
                accentColor: "#ff9800"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UTStatCard {
                title: "Total"
                value: "128"
            }
            UTStatCard {
                title: "NG"
                value: "3"
            }
            UTStatCard {
                title: "OK"
                value: "125"
            }
            UTStatCard {
                title: "Correct"
                value: "0"
            }
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }
}
