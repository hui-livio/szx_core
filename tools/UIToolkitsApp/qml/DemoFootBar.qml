/*
 * @file DemoFootBar.qml
 * @brief UIToolkits 的 UTFootBar 组件演示页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null

    readonly property var theme: UTTheme.theme

    contentWidth: contentColumn.width
    contentHeight: contentColumn.implicitHeight

    ColumnLayout {
        id: contentColumn
        width: root.width - 14
        spacing: 16

        UTLabel {
            text: qsTr("Foot Bar")
            textRole: UTLabel.TextRole.H2
            color: root.theme.primaryTextColor
        }

        UTLabel {
            Layout.fillWidth: true
            text: qsTr("UTFootBar — 标准窗口底部信息栏，支持状态、辅助信息和自定义三区内容。")
            textRole: UTLabel.TextRole.Caption
            color: root.theme.secondaryTextColor
            wrapMode: Text.WordWrap
        }

        UTLabel {
            text: qsTr("基础样式")
            textRole: UTLabel.TextRole.H4
            color: root.theme.primaryTextColor
        }

        UTFootBar {
            Layout.fillWidth: true
            statusText: qsTr("Ready")
            statusTone: UTFootBar.StatusTone.Success
            centerText: qsTr("AOI inspection system")
            rightText: qsTr("v1.0.0")
        }

        UTLabel {
            text: qsTr("状态语义")
            textRole: UTLabel.TextRole.H4
            color: root.theme.primaryTextColor
        }

        UTFootBar {
            Layout.fillWidth: true
            statusText: qsTr("Connecting camera…")
            statusTone: UTFootBar.StatusTone.Info
            rightText: qsTr("Camera 1")
        }

        UTFootBar {
            Layout.fillWidth: true
            statusText: qsTr("Waiting for PLC")
            statusTone: UTFootBar.StatusTone.Warning
            centerText: qsTr("Line 2")
            rightText: qsTr("Retrying")
        }

        UTFootBar {
            Layout.fillWidth: true
            statusText: qsTr("Device disconnected")
            statusTone: UTFootBar.StatusTone.Danger
            centerText: qsTr("Manual intervention required")
            rightText: qsTr("Error E-104")
        }

        UTLabel {
            text: qsTr("无状态指示器")
            textRole: UTLabel.TextRole.H4
            color: root.theme.primaryTextColor
        }

        UTFootBar {
            Layout.fillWidth: true
            showStatusIndicator: false
            statusText: qsTr("Inspection completed")
            centerText: qsTr("128 boards processed")
            rightText: qsTr("17:30")
        }

        UTLabel {
            text: qsTr("自定义内容")
            textRole: UTLabel.TextRole.H4
            color: root.theme.primaryTextColor
        }

        UTFootBar {
            Layout.fillWidth: true
            statusText: qsTr("Running")
            statusTone: UTFootBar.StatusTone.Success

            centerContent: [
                UTProgressBar {
                    width: 220
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0
                    to: 100
                    value: 68
                }
            ]

            rightContent: [
                UTButton {
                    anchors.verticalCenter: parent.verticalCenter
                    implicitHeight: 26
                    text: qsTr("View details")
                    buttonType: UTButton.ButtonType.Text
                    onClicked: root.toast && root.toast.showToast(qsTr("FootBar details"), UTToast.ToastType.Info, 1200)
                }
            ]
        }

        Item {
            Layout.preferredHeight: 8
        }
    }
}
