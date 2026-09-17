/*
 * @file UTSpinBox.qml
 * @brief UIToolkits 的 UTSpinBox.qml 组件。
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes
import UIToolkits

SpinBox {
    id: root

    property alias horizontalAlignment: valueInput.horizontalAlignment
    property bool keyboardTracking: false
    property bool buttonsVisible: false
    property int stepModifier: Qt.NoModifier
    property bool updatingFromInput: false
    property real wheelRemainder: 0
    property alias acceptableInput: valueInput.acceptableInput
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
    rightPadding: buttonsVisible ? 28 : 12
    topPadding: 8
    bottomPadding: 8
    font.pixelSize: 14

    function commitInput() {
        if (!valueInput.acceptableInput) return;
        const next = root.valueFromText(valueInput.text, root.locale);
        if (next !== root.value) {
            root.value = next;
            root.valueModified();
        }
    }
    function resetInput() {
        valueInput.text = root.textFromValue(root.value, root.locale);
    }
    function stepInput(steps, modifiers) {
        root.commitInput();
        const multiplier = root.stepModifier !== Qt.NoModifier && (modifiers & root.stepModifier) ? 10 : 1;
        const minimum = Math.min(root.from, root.to);
        const maximum = Math.max(root.from, root.to);
        const candidate = root.value + steps * root.stepSize * multiplier * (root.from > root.to ? -1 : 1);
        const next = root.wrap ? (candidate > maximum ? minimum : candidate < minimum ? maximum : candidate)
                               : Math.max(minimum, Math.min(maximum, candidate));
        if (next !== root.value) {
            root.value = next;
            root.valueModified();
        }
        root.resetInput();
        valueInput.selectAll();
    }
    valueFromText: function(text, locale) {
        if (!valueInput.acceptableInput) return root.value;
        return Number.fromLocaleString(locale, text);
    }
    onDisplayTextChanged: {
        if (valueInput && !root.updatingFromInput) valueInput.text = root.displayText;
    }

    contentItem: TextInput {
        id: valueInput
        Component.onCompleted: text = root.displayText
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
        onTextEdited: {
            if (!root.keyboardTracking || !acceptableInput) return;
            root.updatingFromInput = true;
            root.commitInput();
            root.updatingFromInput = false;
        }
        onEditingFinished: {
            root.commitInput();
            root.resetInput();
        }
        onActiveFocusChanged: {
            if (!activeFocus) {
                root.commitInput();
                root.resetInput();
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

    component StepButton: UTButton {
        id: stepButton
        required property int direction
        property int modifiers: Qt.NoModifier
        visible: root.buttonsVisible
        x: root.width - width - 2
        width: root.buttonsVisible ? 24 : 0
        height: root.buttonsVisible ? (root.height - 4) / 2 : 0
        implicitWidth: width
        implicitHeight: height
        padding: 0
        text: direction > 0 ? "▴" : "▾"
        buttonType: UTButton.ButtonType.Text
        font.pixelSize: 12
        focusPolicy: Qt.NoFocus
        autoRepeat: true
        autoRepeatDelay: 500
        autoRepeatInterval: 100
        contentItem: Item {
            implicitWidth: 8
            implicitHeight: 5
            Shape {
                anchors.centerIn: parent
                width: 8
                height: 5
                ShapePath {
                    fillColor: stepButton.enabled ? stepButton.primaryColor : stepButton.disabledColor
                    strokeWidth: 0
                    startX: 0
                    startY: stepButton.direction > 0 ? 5 : 0
                    PathLine { x: 4; y: stepButton.direction > 0 ? 0 : 5 }
                    PathLine { x: 8; y: stepButton.direction > 0 ? 5 : 0 }
                    PathLine { x: 0; y: stepButton.direction > 0 ? 5 : 0 }
                }
            }
        }
        onPressed: {
            root.forceActiveFocus(Qt.MouseFocusReason);
            root.stepInput(direction, modifiers);
        }
        // Capture modifiers before UTButton handles the press and auto-repeat.
        MouseArea {
            anchors.fill: parent
            onPressed: mouse => {
                stepButton.modifiers = mouse.modifiers;
                mouse.accepted = false;
            }
        }
    }
    up.indicator: StepButton {
        objectName: "spinIncrease"
        direction: 1
        y: 2
    }
    down.indicator: StepButton {
        objectName: "spinDecrease"
        direction: -1
        y: root.height / 2
    }

    WheelHandler {
        enabled: root.enabled && (root.activeFocus || root.wheelEnabled)

        onWheel: function(event) {
            const delta = event.angleDelta.y || event.angleDelta.x;
            root.wheelRemainder += delta;
            const steps = Math.trunc(root.wheelRemainder / 120);
            root.wheelRemainder -= steps * 120;
            if (steps !== 0) root.stepInput(steps, event.modifiers);
            event.accepted = true;
        }
    }

    Keys.priority: Keys.BeforeItem
    Keys.onUpPressed: function(event) {
        if (root.enabled) {
            root.stepInput(1, event.modifiers);
            event.accepted = true;
        }
    }
    Keys.onDownPressed: function(event) {
        if (root.enabled) {
            root.stepInput(-1, event.modifiers);
            event.accepted = true;
        }
    }
    Keys.onPressed: event => {
        if (root.enabled && root.buttonsVisible && (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown)) {
            root.stepInput(event.key === Qt.Key_PageUp ? 10 : -10, Qt.NoModifier);
            event.accepted = true;
        }
    }
}
