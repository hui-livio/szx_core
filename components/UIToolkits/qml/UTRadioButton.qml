/*
 * @file UTRadioButton.qml
 * @brief UIToolkits themed, mutually-exclusive radio button.
 */
import QtQuick
import QtQuick.Controls.Basic

RadioButton {
    id: root
    spacing: 8
    font.pixelSize: 14
    leftPadding: 4
    topPadding: 4
    bottomPadding: 4

    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: root.leftPadding
        y: root.height / 2 - height / 2
        radius: width / 2
        color: "transparent"
        border.width: 2
        border.color: root.enabled ? (root.checked ? UTTheme.primaryColor : "#8b93a3") : "#555b68"
        Rectangle {
            anchors.centerIn: parent
            width: 10
            height: 10
            radius: width / 2
            color: UTTheme.primaryColor
            visible: root.checked
        }
    }

    contentItem: Text {
        text: root.text
        color: root.enabled ? UTTheme.primaryTextColor : "#8b93a3"
        font: root.font
        leftPadding: root.indicator.width + root.spacing
        verticalAlignment: Text.AlignVCenter
    }
}
