/*
 * @file UTMessageDialog.qml
 * @brief UIToolkits 的 UTMessageDialog.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

UTDialog {
    id: root

    // ── Theme ────────────────────────────────────────────────────
    readonly property bool isDark: UTTheme.isDark === true

    // ── API ──────────────────────────────────────────────────────
    /// 消息正文
    property alias messageText: messageLabel.text
    /// 标准按钮组合，默认 Ok | Cancel
    property int dialogButtons: Dialog.Ok | Dialog.Cancel
    /// 否定按钮文案，可由应用侧覆盖以使用应用翻译上下文
    property string rejectButtonText: dialogButtons & Dialog.Cancel ? qsTr("Cancel") : qsTr("No")
    /// 肯定按钮文案，可由应用侧覆盖以使用应用翻译上下文
    property string acceptButtonText: dialogButtons & Dialog.Ok ? qsTr("Ok") : qsTr("Yes")

    standardButtons: Dialog.NoButton
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    initialDialogWidth: 420
    initialDialogHeight: 220
    minimumDialogWidth: 360
    minimumDialogHeight: 180

    font.family: UTTheme.fontFamily

    // ── 样式 ─────────────────────────────────────────────────────
    // ── Content ──────────────────────────────────────────────────
    dialogContent: ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillHeight: true
        }

        Label {
            id: messageLabel
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: UTTheme.secondaryTextColor
            font.pixelSize: 14
            font.family: root.font.family
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            spacing: 12

            Item { Layout.fillWidth: true }

            // Cancel / No 按钮
            UTButton {
                visible: root.dialogButtons & (Dialog.Cancel | Dialog.No)
                text: root.rejectButtonText
                buttonType: UTButton.ButtonType.Outlined
                implicitWidth: 100
                implicitHeight: 34
                font.pixelSize: 14
                onClicked: {
                    if (root.dialogButtons & Dialog.Cancel)
                        root.reject();
                    else
                        root.reject();
                }
            }

            // Ok / Yes 按钮
            UTButton {
                visible: root.dialogButtons & (Dialog.Ok | Dialog.Yes)
                text: root.acceptButtonText
                buttonType: UTButton.ButtonType.Filled
                implicitWidth: 100
                implicitHeight: 34
                font.pixelSize: 14
                onClicked: root.accept()
            }

            Item { Layout.fillWidth: true }
        }
    }

    // ── Overlay 遮罩 ─────────────────────────────────────────────
    Overlay.modal: Rectangle {
        color: root.isDark ? "#80000000" : "#40000000"
    }
}
