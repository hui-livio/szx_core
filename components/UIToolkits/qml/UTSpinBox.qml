/*
 * @file UTSpinBox.qml
 * @brief UIToolkits 的 UTSpinBox.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

SpinBox {
    id: root

    property string accName: root.displayText
    property int accRole: Accessible.SpinBox
    property color primaryColor: UTTheme.primaryColor
    property color textColor: UTTheme.primaryTextColor
    property color fieldBg: UTTheme.surfaceColor
    property color fieldBgDisabled: Qt.rgba(fieldBg.r * 0.92, fieldBg.g * 0.92, fieldBg.b * 0.92, fieldBg.a)
    property color borderNormal: UTTheme.borderColor
    property color borderHover: primaryColor
    property color borderFocus: primaryColor
    property color borderDisabled: Qt.rgba(borderNormal.r, borderNormal.g, borderNormal.b, borderNormal.a * 0.4)
    Accessible.name: accName
    Accessible.role: accRole

    implicitWidth: 120
    implicitHeight: 40
    activeFocusOnTab: true
    leftPadding: 12
    rightPadding: 12
    topPadding: 8
    bottomPadding: 8
    font.pixelSize: 14

    contentItem: TextInput {
        text: root.displayText
        font: root.font
        color: root.enabled ? root.textColor : root.borderDisabled
        selectionColor: root.primaryColor
        selectedTextColor: UTTheme.primaryTextColor
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        readOnly: !root.editable
        selectByMouse: root.editable
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        validator: root.validator
        onEditingFinished: {
            const next = root.valueFromText(text, root.locale)
            if (next !== root.value) {
                root.value = next
                root.valueModified()
            }
        }
    }

    background: Rectangle {
        id: backgroundRect
        color: root.enabled ? root.fieldBg : root.fieldBgDisabled
        radius: UTTheme.radius || 8
        border.width: root.activeFocus ? 2 : 1
        border.color: {
            if (!root.enabled)
                return root.borderDisabled;
            if (root.activeFocus)
                return root.borderFocus;
            if (hoverHandler.hovered)
                return root.borderHover;
            return root.borderNormal;
        }

        HoverHandler {
            id: hoverHandler
        }

        Behavior on border.width {
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: 200 }
        }
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    up.indicator: Item {
        implicitWidth: 0
        implicitHeight: 0
    }

    down.indicator: Item {
        implicitWidth: 0
        implicitHeight: 0
    }

    WheelHandler {
        enabled: root.enabled && root.activeFocus

        onWheel: function(event) {
            if (event.angleDelta.y === 0)
                return;
            if (event.angleDelta.y > 0)
                root.increase();
            else
                root.decrease();
            event.accepted = true;
        }
    }

    Keys.priority: Keys.BeforeItem
    Keys.onUpPressed: function(event) {
        if (root.enabled) {
            root.increase();
            event.accepted = true;
        }
    }
    Keys.onDownPressed: function(event) {
        if (root.enabled) {
            root.decrease();
            event.accepted = true;
        }
    }
}
