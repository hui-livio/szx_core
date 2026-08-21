/*
 * @file UTImageAvatar.qml
 * @brief UIToolkits 的 UTImageAvatar.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

Control {
    id: root

    enum Shape {
        Circle,
        RoundedRect
    }
    property alias source: avatarImage.source
    property string fallbackText: ""
    property int shape: UTImageAvatar.Shape.Circle
    property int size: 44
    property color backgroundColor: UTTheme.primaryColor
    property color foregroundColor: UTTheme.primaryTextColor

    implicitWidth: size
    implicitHeight: size

    contentItem: Rectangle {
        anchors.fill: parent
        radius: root.shape === UTImageAvatar.Shape.Circle ? width / 2 : 10
        color: root.backgroundColor
        clip: true

        Image {
            id: avatarImage
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            visible: status === Image.Ready && source.toString() !== ""
            smooth: true
        }

        Label {
            anchors.centerIn: parent
            text: root.fallbackText
            visible: !avatarImage.visible
            color: root.foregroundColor
            font.pixelSize: Math.max(12, Math.round(root.size * 0.36))
            font.weight: Font.Medium
        }
    }
}
