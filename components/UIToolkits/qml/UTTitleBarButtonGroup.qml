/*
 * @file UTTitleBarButtonGroup.qml
 * @brief UIToolkits 的 UTTitleBarButtonGroup.qml 组件。
 */

import QtQuick

Item {
    id: root

    property color hoverColor: Qt.rgba(1, 1, 1, 0.08)
    property color pressedColor: Qt.rgba(1, 1, 1, 0.15)
    property color iconColor: "#FFFFFF"
    property bool maximized: false
    property bool minVisible: true
    property bool maxVisible: true
    property bool closeVisible: true
    property url minIcon: ""
    property url maxIcon: ""
    property url closeIcon: ""
    property int iconSize: 24
    property color closeHoverColor: "#e81123"
    property color closePressedColor: "#c50e1e"
    property color closeIconColor: "#ffffff"

    signal minimizeClicked
    signal maximizeClicked
    signal closeClicked

    implicitWidth: defaultRow.implicitWidth
    implicitHeight: 28

    readonly property string _svgMin: "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 1024 1024'><path d='M960 544H64a32 32 0 1 1 0-64h896a32 32 0 1 1 0 64z'/></svg>"
    readonly property string _svgMax: "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 1024 1024'><rect x='192' y='192' width='640' height='640' rx='64' fill='none' stroke='#fff' stroke-width='64'/></svg>"
    readonly property string _svgRestore: "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 1024 1024'><path d='M794.432 983.552H51.2a25.6 25.6 0 01-25.6-25.6V214.784a25.6 25.6 0 0125.6-25.6h152.768V66.112a25.6 25.6 0 0125.6-25.6H972.8a25.6 25.6 0 0125.6 25.6v743.232a25.6 25.6 0 01-25.6 25.6h-152.768v123.008a25.6 25.6 0 01-25.6 25.6zm-717.632-51.2h692.032V240.384H76.8v691.968zm743.232-148.672H947.2V91.648H255.168v97.472h539.264a25.6 25.6 0 0125.6 25.6v568.96z'/></svg>"
    readonly property string _svgClose: "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 1024 1024'><path d='M557.3 512L883.7 185.7c12.5-12.5 12.5-32.8 0-45.3s-32.8-12.5-45.3 0L512 466.7 185.7 140.3c-12.5-12.5-32.8-12.5-45.3 0s-12.5 32.8 0 45.3L466.7 512 140.3 838.3c-12.5 12.5-12.5 32.8 0 45.3 6.2 6.2 14.4 9.4 22.6 9.4s16.4-3.1 22.6-9.4L512 557.3l326.3 326.3c6.2 6.2 14.4 9.4 22.6 9.4s16.4-3.1 22.6-9.4c12.5-12.5 12.5-32.8 0-45.3L557.3 512z'/></svg>"

    Row {
        id: defaultRow
        spacing: 4

        UTSvgButton {
            id: btnMin
            objectName: "btnMin"

            visible: root.minVisible
            width: 32
            height: 28
            svgSource: root.minIcon.toString() !== "" ? root.minIcon : root._svgMin
            svgColor: root.iconColor
            svgSize: Qt.size(root.iconSize, root.iconSize)
            bgHoverColor: root.hoverColor
            bgPressedColor: root.pressedColor
            tooltipText: qsTr("Minimize")
            onClicked: root.minimizeClicked()
        }

        UTSvgButton {
            id: btnMax
            objectName: "btnMax"

            visible: root.maxVisible
            width: 32
            height: 28
            svgSource: root.maxIcon.toString() !== "" ? root.maxIcon : (root.maximized ? root._svgRestore : root._svgMax)
            svgColor: root.iconColor
            svgSize: Qt.size(root.iconSize, root.iconSize)
            bgHoverColor: root.hoverColor
            bgPressedColor: root.pressedColor
            tooltipText: root.maximized ? qsTr("Restore") : qsTr("Maximize")
            onClicked: root.maximizeClicked()
        }

        UTSvgButton {
            id: btnClose
            objectName: "btnClose"

            visible: root.closeVisible
            width: 32
            height: 28
            svgSource: root.closeIcon.toString() !== "" ? root.closeIcon : root._svgClose
            svgColor: hovered ? root.closeIconColor : root.iconColor
            svgSize: Qt.size(root.iconSize, root.iconSize)
            bgHoverColor: root.closeHoverColor
            bgPressedColor: root.closePressedColor
            tooltipText: qsTr("Close")
            onClicked: root.closeClicked()
        }
    }
}
