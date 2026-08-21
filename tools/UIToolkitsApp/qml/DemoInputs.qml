/*
 * @file DemoInputs.qml
 * @brief UIToolkits 展示应用的 DemoInputs.qml 页面。
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
            text: "Text Input"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLineEdit {
            placeholderText: "Normal input"
            Layout.fillWidth: true
        }

        UTLineEdit {
            placeholderText: "With error"
            errorText: "This field is required"
            Layout.fillWidth: true
        }

        UTLineEdit {
            placeholderText: "Password input"
            passwordMode: true
            Layout.fillWidth: true
        }

        UTLabel {
            text: "ComboBox"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTCombobox {
            Layout.fillWidth: true
            model: ["Option 1", "Option 2", "Option 3", "Option 4", "Option 5"]
            onActivated: function (index) {
                if (root.toast)
                    root.toast.showToast("Selected: " + currentText, UTToast.ToastType.Info, 1500);
            }
        }

        UTCombobox {
            Layout.fillWidth: true
            currentIndex: 2
            textRole: "text"
            model: ListModel {
                ListElement {
                    text: "Basic"
                }
                ListElement {
                    text: "Standard"
                }
                ListElement {
                    text: "Premium"
                }
                ListElement {
                    text: "Enterprise"
                }
            }
        }

        UTCombobox {
            Layout.fillWidth: true
            enabled: false
            model: ["Disabled State"]
        }

        UTLabel {
            text: "Editable ComboBox"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTCombobox {
            Layout.fillWidth: true
            editable: true
            placeholderText: "Type or select..."
            model: ["Apple", "Banana", "Cherry", "Durian", "Elderberry", "Fig", "Grape"]
            onActivated: function (index) {
                if (root.toast)
                    root.toast.showToast("Selected: " + currentText, UTToast.ToastType.Info, 1500);
            }
            onAccepted: function () {
                if (root.toast)
                    root.toast.showToast("Edit accepted: " + editText, UTToast.ToastType.Success, 1500);
            }
        }

        UTCombobox {
            Layout.fillWidth: true
            editable: true
            currentIndex: 0
            textRole: "text"
            model: ListModel {
                ListElement {
                    text: "Light"
                }
                ListElement {
                    text: "Dark"
                }
                ListElement {
                    text: "Auto"
                }
            }
            onActivated: function (index) {
                if (root.toast)
                    root.toast.showToast("Theme: " + currentText, UTToast.ToastType.Info, 1500);
            }
            onAccepted: function () {
                if (root.toast)
                    root.toast.showToast("Edit accepted: " + editText, UTToast.ToastType.Success, 1500);
            }
        }

        UTLabel {
            text: "ScrollBar"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 180
            color: "transparent"

            ListView {
                id: scrollDemo
                anchors.fill: parent
                anchors.rightMargin: scrollDemoBar.visible ? scrollDemoBar.thickness + 4 : 0
                clip: true
                model: 20
                delegate: UTLabel {
                    width: ListView.view.width - 20
                    text: "Item " + (index + 1)
                    textRole: UTLabel.TextRole.Body
                }
            }

            UTScrollBar {
                id: scrollDemoBar
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                target: scrollDemo
            }
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }
}
