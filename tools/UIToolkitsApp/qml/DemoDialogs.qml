/*
 * @file DemoDialogs.qml
 * @brief UIToolkits 展示应用的 DemoDialogs.qml 页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

Item {
    id: root

    property var toast: null
    readonly property var theme: UTTheme.theme

    ColumnLayout {
        anchors.fill: parent
        spacing: 20

        UTLabel {
            text: "Dialogs"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "Draggable, resizable UTDialog and themed confirmation dialog."
            textRole: UTLabel.TextRole.Body
            color: theme.secondaryTextColor
        }

        RowLayout {
            spacing: 12

            UTButton {
                text: "Open resizable dialog"
                buttonType: UTButton.ButtonType.Filled
                onClicked: previewDialog.open()
            }

            UTButton {
                text: "Open confirmation"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: messageDialog.open()
            }
        }

        UTCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 150

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                UTLabel {
                    text: "Verification"
                    textRole: UTLabel.TextRole.H4
                }
                UTLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Drag the title bar, resize from all lower and side edges, and press Esc or Close to dismiss. Dialog events are recorded through UTLogger."
                    color: theme.secondaryTextColor
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    UTDialog {
        id: previewDialog
        titleText: "Resizable UTDialog"
        initialDialogWidth: 560
        initialDialogHeight: 360
        minimumDialogWidth: 360
        minimumDialogHeight: 220

        dialogContent: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            UTLabel {
                text: "Resize this dialog from its edges or corners."
                textRole: UTLabel.TextRole.H4
            }
            UTTextArea {
                Layout.fillWidth: true
                Layout.fillHeight: true
                placeholderText: "Type here to verify focus and resizing."
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                UTButton {
                    text: "Close"
                    buttonType: UTButton.ButtonType.Outlined
                    onClicked: previewDialog.close()
                }
            }
        }
    }

    UTMessageDialog {
        id: messageDialog
        titleText: "Confirmation"
        messageText: "This is a themed UTMessageDialog."
        dialogButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: root.toast && root.toast.showToast("Accepted", UTToast.ToastType.Success, 1200)
        onRejected: root.toast && root.toast.showToast("Cancelled", UTToast.ToastType.Info, 1200)
    }
}
