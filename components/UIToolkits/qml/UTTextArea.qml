/*
 * @file UTTextArea.qml
 * @brief UIToolkits 的 UTTextArea.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

TextArea {
    id: root

    property color textColor: UTTheme.primaryTextColor
    property color placeholderColor: UTTheme.secondaryTextColor
    property color fieldColor: UTTheme.surfaceColor
    property color borderNormal: UTTheme.borderColor
    property color borderFocus: UTTheme.primaryColor

    implicitWidth: 240
    implicitHeight: 120
    padding: 10
    color: enabled ? textColor : UTTheme.secondaryTextColor
    placeholderTextColor: placeholderColor
    wrapMode: TextEdit.Wrap
    selectByMouse: true

    background: Rectangle {
        radius: UTTheme.radius || 8
        color: root.fieldColor
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? root.borderFocus : root.borderNormal
    }
}
