/*
 * @file UTTextArea.qml
 * @brief UIToolkits 的 UTTextArea.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

ScrollView {
    id: root

    property alias text: editor.text
    property alias readOnly: editor.readOnly
    property alias placeholderText: editor.placeholderText
    property alias textColor: editor.color
    property alias selectByMouse: editor.selectByMouse
    property alias wrapMode: editor.wrapMode
    property alias textFormat: editor.textFormat
    property alias cursorPosition: editor.cursorPosition
    property color fieldColor: UTTheme.surfaceColor
    property color borderNormal: UTTheme.borderColor
    property color borderFocus: UTTheme.primaryColor
    property color scrollbarTrackColor: UTTheme.scrollbarTrackColor
    property color scrollbarThumbNormalColor: UTTheme.scrollbarThumbNormalColor
    property color scrollbarThumbHoverColor: UTTheme.scrollbarThumbHoverColor
    property color scrollbarThumbPressedColor: UTTheme.scrollbarThumbPressedColor

    implicitWidth: 240
    implicitHeight: 120
    activeFocusOnTab: true
    clip: true
    contentWidth: editor.wrapMode === TextEdit.NoWrap
                  ? Math.max(availableWidth, editor.contentWidth + editor.leftPadding + editor.rightPadding)
                  : editor.width
    contentHeight: editor.height

    TextArea {
        id: editor

        width: root.availableWidth
        height: Math.max(root.availableHeight, implicitHeight)
        padding: 10
        color: enabled ? UTTheme.primaryTextColor : UTTheme.secondaryTextColor
        placeholderTextColor: UTTheme.secondaryTextColor
        font: root.font
        wrapMode: TextEdit.Wrap
        selectByMouse: true
        background: Item {}
        focus: root.activeFocus

        onActiveFocusChanged: {
            if (activeFocus)
                root.forceActiveFocus();
        }
    }

    onActiveFocusChanged: {
        if (activeFocus)
            editor.forceActiveFocus();
    }

    Keys.onPressed: event => {
        const flickable = root.contentItem;
        if (!flickable || flickable.contentHeight <= flickable.height)
            return;

        let nextY = flickable.contentY;
        switch (event.key) {
        case Qt.Key_PageDown:
            nextY += flickable.height;
            break;
        case Qt.Key_PageUp:
            nextY -= flickable.height;
            break;
        case Qt.Key_Home:
            nextY = 0;
            break;
        case Qt.Key_End:
            nextY = flickable.contentHeight - flickable.height;
            break;
        default:
            return;
        }
        flickable.contentY = Math.max(0, Math.min(nextY, flickable.contentHeight - flickable.height));
        event.accepted = true;
    }

    ScrollBar.vertical: ScrollBar {
        id: verticalScrollBar
        objectName: "verticalScrollBar"
        parent: root
        x: root.mirrored ? 0 : root.width - width
        y: root.topPadding
        height: root.availableHeight
        z: 2
        policy: ScrollBar.AlwaysOn
        visible: true
        interactive: true
        contentItem: Rectangle {
            implicitWidth: 10
            radius: 5
            color: root.scrollbarTrackColor

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: parent.radius
                color: verticalScrollBar.pressed
                       ? root.scrollbarThumbPressedColor
                       : verticalScrollBar.hovered
                         ? root.scrollbarThumbHoverColor
                         : root.scrollbarThumbNormalColor
            }
        }
    }

    ScrollBar.horizontal: ScrollBar {
        id: horizontalScrollBar
        parent: root
        x: root.leftPadding
        y: root.height - height
        width: root.availableWidth
        z: 2
        policy: ScrollBar.AlwaysOn
        visible: root.contentWidth > root.availableWidth
        interactive: true
        contentItem: Rectangle {
            implicitHeight: 10
            radius: 5
            color: root.scrollbarTrackColor

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: parent.radius
                color: horizontalScrollBar.pressed
                       ? root.scrollbarThumbPressedColor
                       : horizontalScrollBar.hovered
                         ? root.scrollbarThumbHoverColor
                         : root.scrollbarThumbNormalColor
            }
        }
    }

    background: Rectangle {
        radius: UTTheme.radius || 8
        color: root.fieldColor
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? root.borderFocus : root.borderNormal
    }
}
