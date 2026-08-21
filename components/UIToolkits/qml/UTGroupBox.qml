/*
 * @file UTGroupBox.qml
 * @brief UIToolkits 的 UTGroupBox.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

GroupBox {
    id: root

    topPadding: 28
    leftPadding: 12
    rightPadding: 12
    bottomPadding: 12

    label: UTLabel {
        x: root.leftPadding - leftPadding
        y: Math.max(0, (root.topPadding - implicitHeight) / 2)
        leftPadding: 4
        rightPadding: 4
        text: root.title
        textRole: UTLabel.TextRole.H6
        color: UTTheme.primaryTextColor
        background: Rectangle {
            color: UTTheme.surfaceColor
        }
    }

    background: Rectangle {
        y: root.topPadding / 2
        height: root.height - y
        radius: UTTheme.radius || 8
        color: "transparent"
        border.width: 1
        border.color: UTTheme.borderColor
    }
}
