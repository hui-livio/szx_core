/*
 * @file DemoButtons.qml
 * @brief UIToolkits 展示应用的 DemoButtons.qml 页面。
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
            text: "Buttons"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "Filled / Tonal / Outlined / Text"
            textRole: UTLabel.TextRole.Body
            color: theme.secondaryTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UTButton {
                text: "Filled"
                buttonType: UTButton.ButtonType.Filled
            }
            UTButton {
                text: "Tonal"
                buttonType: UTButton.ButtonType.Tonal
            }
            UTButton {
                text: "Outlined"
                buttonType: UTButton.ButtonType.Outlined
            }
            UTButton {
                text: "Text"
                buttonType: UTButton.ButtonType.Text
            }
        }

        UTLabel {
            text: "Round Buttons"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        Flow {
            Layout.fillWidth: true
            spacing: 12

            UTRoundButton {
                text: "+"
                variant: UTRoundButton.ButtonVariant.Filled
                buttonSize: 48
                onClicked: root.toast && root.toast.showToast("Filled Round", UTToast.ToastType.Info, 1000)
            }
            UTRoundButton {
                text: "★"
                variant: UTRoundButton.ButtonVariant.Tonal
                buttonSize: 48
                onClicked: root.toast && root.toast.showToast("Tonal Round", UTToast.ToastType.Info, 1000)
            }
            UTRoundButton {
                text: "✎"
                variant: UTRoundButton.ButtonVariant.Outlined
                buttonSize: 48
                onClicked: root.toast && root.toast.showToast("Outlined Round", UTToast.ToastType.Info, 1000)
            }
            UTRoundButton {
                text: "♥"
                variant: UTRoundButton.ButtonVariant.Text
                buttonSize: 48
                onClicked: root.toast && root.toast.showToast("Text Round", UTToast.ToastType.Info, 1000)
            }
            UTRoundButton {
                text: "S"
                variant: UTRoundButton.ButtonVariant.Filled
                buttonSize: 32
            }
            UTRoundButton {
                text: "L"
                variant: UTRoundButton.ButtonVariant.Filled
                buttonSize: 64
            }
            UTRoundButton {
                text: "Off"
                variant: UTRoundButton.ButtonVariant.Filled
                buttonSize: 48
                enabled: false
            }

            UTRoundButton {
                variant: UTRoundButton.ButtonVariant.Filled
                buttonSize: 48
                iconSource: "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'><path d='M19 13H13V19H11V13H5V11H11V5H13V11H19V13Z'/></svg>"
                onClicked: root.toast && root.toast.showToast("SVG Filled", UTToast.ToastType.Success, 1000)
            }
            UTRoundButton {
                variant: UTRoundButton.ButtonVariant.Outlined
                buttonSize: 48
                iconSource: "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'><path d='M3 18H21V16H3V18ZM3 13H21V11H3V13ZM3 6V8H21V6H3Z'/></svg>"
                onClicked: root.toast && root.toast.showToast("SVG Outlined", UTToast.ToastType.Success, 1000)
            }
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }
}
