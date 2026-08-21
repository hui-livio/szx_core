/*
 * @file UTTitleBar.qml
 * @brief UIToolkits 的 UTTitleBar.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: root

    property var windowObj: null
    property bool draggable: true

    property string titleText: ""
    property url iconSource: ""
    property int iconSize: 20
    property int iconWidth: iconSize
    property int iconHeight: iconSize
    property color titleBarColor: UTTheme.titleBarColor || UTTheme.backgroundColor
    property color textColor: UTTheme.primaryTextColor
    property color iconColor: {
        if (UTTheme.isDark)
            return "#FFFFFF";
        return UTTheme.secondaryTextColor;
    }

    property bool showDefaultButtons: true
    property bool minButtonVisible: true
    property bool maxButtonVisible: true
    property bool closeButtonVisible: true
    property url closeButtonIcon: ""
    property int buttonIconSize: 26
    property int systemButtonsRightMargin: 6
    property bool doubleClickMaximizeEnabled: true
    readonly property bool fillsAvailableScreen: {
        if (windowObj === null || windowObj.screen === undefined || windowObj.screen === null)
            return false

        const area = windowObj.screen.availableGeometry
        if (area === undefined || area === null)
            return false

        const tolerance = 2
        return Math.abs(windowObj.x - area.x) <= tolerance
                && Math.abs(windowObj.y - area.y) <= tolerance
                && Math.abs(windowObj.width - area.width) <= tolerance
                && Math.abs(windowObj.height - area.height) <= tolerance
    }
    readonly property bool maximized: windowObj !== null
                                      && (windowObj.visibility === Window.Maximized
                                          || (windowObj.windowStates & Qt.WindowMaximized) !== 0
                                          || fillsAvailableScreen)

    implicitWidth: 960
    implicitHeight: 36

    signal minimizeClicked
    signal maximizeClicked
    signal closeClicked

    // ── Background ──────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: root.titleBarColor
    }

    MouseArea {
        id: dragArea
        objectName: "dragArea"
        anchors.fill: parent
        enabled: root.draggable && root.windowObj !== null
        z: -1
        property point _p: Qt.point(0, 0)
        onPressed: function (m) {
            _p = Qt.point(m.x, m.y);
        }
        onPositionChanged: function (m) {
            if (root.windowObj && !root.maximized) {
                root.windowObj.x += m.x - _p.x;
                root.windowObj.y += m.y - _p.y;
            }
        }
        onDoubleClicked: {
            if (root.windowObj && root.doubleClickMaximizeEnabled) {
                root.windowObj.visibility = root.maximized ? Window.Windowed : Window.Maximized;
            }
        }
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: 4

        Item {
            Layout.preferredWidth: (leftContent.children.length > 0 ? leftContent.implicitWidth : (iconItem.visible ? 80 : 60)) + 12
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            clip: true

            Row {
                id: leftContent
                spacing: 8
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter

                Item {
                    id: iconItem
                    visible: root.iconSource.toString() !== ""
                    width: root.iconWidth
                    height: root.iconHeight
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        id: iconImage
                        anchors.fill: parent
                        source: root.iconSource
                        sourceSize: Qt.size(root.iconWidth * 2, root.iconHeight * 2)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                }

                Text {
                    text: root.titleText
                    font.pixelSize: 13
                    font.weight: 500
                    font.family: root.windowObj ? root.windowObj.font.family : ""
                    color: root.textColor
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                }
            }
        }

        // ── 中间：弹性占位 ──────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Item {
            Layout.preferredWidth: rightBar.implicitWidth + 16
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

            Row {
                id: rightBar
                spacing: 4
                anchors.right: parent.right
                anchors.rightMargin: root.systemButtonsRightMargin
                anchors.verticalCenter: parent.verticalCenter

                // ── Default system buttons ──────────────────────────
                UTTitleBarButtonGroup {
                    id: defaultBtnGroup
                    objectName: "defaultBtnGroup"
                    visible: root.showDefaultButtons

                    iconColor: root.iconColor
                    maximized: root.maximized
                    minVisible: root.minButtonVisible
                    maxVisible: root.maxButtonVisible
                    closeVisible: root.closeButtonVisible
                    closeIcon: root.closeButtonIcon
                    iconSize: root.buttonIconSize
                    onMinimizeClicked: root.minimizeClicked()
                    onMaximizeClicked: root.maximizeClicked()
                    onCloseClicked: root.closeClicked()
                }
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Qt.rgba(1, 1, 1, 0.06)
    }
}
