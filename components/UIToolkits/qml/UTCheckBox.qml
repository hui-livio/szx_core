/*
 * @file UTCheckBox.qml
 * @brief UIToolkits 的 UTCheckBox.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

CheckBox {
    id: root

    spacing: 12
    font.pixelSize: 14
    leftPadding: 4
    topPadding: 4
    bottomPadding: 4

    // 无障碍
    property string accName: root.text
    property int accRole: Accessible.CheckBox
    Accessible.name: accName
    Accessible.role: accRole

    indicator: Rectangle {
        implicitWidth: 24
        implicitHeight: 24
        x: root.leftPadding
        y: root.height / 2 - height / 2
        radius: Math.round((UTTheme.radius || 8) * 0.5)
        color: {
            if (!root.enabled) {
                return "#e0e0e0";
            }
            if (root.checked) {
                return UTTheme.primaryColor;
            }
            if (root.hovered) {
                return "#f5f5f5";
            }
            return "#ffffff";
        }
        border.width: root.activeFocus ? 3 : (root.checked ? 0 : 2)
        border.color: root.activeFocus ? (UTTheme.primaryColor) : (root.checked ? "transparent" : (root.hovered ? (UTTheme.primaryColor) : "#bdbdbd"))

        Behavior on color {
            ColorAnimation {
                duration: 200
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 160
            }
        }
        Behavior on border.width {
            NumberAnimation {
                duration: 120
            }
        }

        Text {
            anchors.centerIn: parent
            text: "✓"
            color: "#ffffff"
            font.pixelSize: 16
            font.bold: true
            visible: root.checked
        }
    }

    contentItem: Text {
        text: root.text
        color: root.enabled ? (UTTheme.primaryTextColor) : "#bdbdbd"
        font: root.font
        leftPadding: root.indicator.width + root.spacing
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    background: Rectangle {
        radius: (UTTheme.radius || 8) * 0.5
        color: "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: root.visualFocus ? (UTTheme.primaryColor) : "transparent"
        Behavior on border.color {
            ColorAnimation {
                duration: 160
            }
        }
        Behavior on border.width {
            NumberAnimation {
                duration: 120
            }
        }
    }
}
