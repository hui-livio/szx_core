/*
 * @file UTMenuBar.qml
 * @brief UIToolkits 的主题化菜单栏组件。
 *
 * 菜单弹出内容复用 UTMenu 的右键菜单视觉契约：surface 背景、边框、圆角、
 * primary 高亮和统一的文字/禁用态颜色。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MenuBar {
    id: root

    property color barColor: UTTheme.titleBarColor || UTTheme.backgroundColor
    property color textColor: UTTheme.primaryTextColor
    property color activeTextColor: UTTheme.primaryColor
    property color hoverColor: Qt.rgba(UTTheme.primaryColor.r,
                                       UTTheme.primaryColor.g,
                                       UTTheme.primaryColor.b, 0.10)
    property color activeColor: Qt.rgba(UTTheme.primaryColor.r,
                                        UTTheme.primaryColor.g,
                                        UTTheme.primaryColor.b, 0.14)
    property string fontFamily: UTTheme.fontFamily
    property int fontPixelSize: 13

    implicitHeight: 36
    spacing: 2
    contentItem: RowLayout {
        anchors.fill: parent
        spacing: root.spacing
        // MenuBar exposes an object model for its menu items.  Repeater cannot
        // retain an explicit delegate for that model in Qt 6; ListView is the
        // supported content presenter and keeps the MenuBar delegate intact.
        ListView {
            id: menuItems
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            interactive: false
            spacing: root.spacing
            model: root.contentModel
            delegate: root.delegate
        }
    }

    background: Rectangle {
        color: root.barColor
    }

    delegate: MenuBarItem {
        id: menuItem

        implicitWidth: Math.max(58, implicitContentWidth + 24)
        implicitHeight: Math.max(32, root.height - 8)
        Layout.alignment: Qt.AlignVCenter
        leftPadding: 12
        rightPadding: 12
        topPadding: 4
        bottomPadding: 4
        font.family: root.fontFamily
        font.pixelSize: 13
        font.weight: highlighted ? Font.DemiBold : Font.Normal

        contentItem: Text {
            text: menuItem.text
            color: menuItem.enabled
                   ? (menuItem.highlighted ? root.activeTextColor : root.textColor)
                   : UTTheme.secondaryTextColor
            font.family: root.fontFamily
            font.pixelSize: root.fontPixelSize
            font.weight: menuItem.highlighted ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: Math.max(4, (UTTheme.radius || 8) - 2)
            color: menuItem.highlighted
                   ? (menuItem.checked ? root.activeColor : root.hoverColor)
                   : "transparent"
            border.width: menuItem.activeFocus ? 1 : 0
            border.color: root.activeTextColor
        }

        HoverHandler {
            cursorShape: menuItem.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }
}
