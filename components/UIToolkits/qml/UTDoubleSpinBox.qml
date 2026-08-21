/*
 * @file UTDoubleSpinBox.qml
 * @brief UIToolkits 的 UTDoubleSpinBox.qml 组件。
 */

import QtQuick

FocusScope {
    id: root

    property real from: 0
    property real to: 100
    property real value: 0
    property real stepSize: 1
    property int decimals: 2
    readonly property int scaleFactor: Math.pow(10, Math.max(0, decimals))
    readonly property real effectiveValue: Math.round(Math.max(from, Math.min(to, value)) * scaleFactor) / scaleFactor

    signal valueModified(real value)

    implicitWidth: spinBox.implicitWidth
    implicitHeight: spinBox.implicitHeight
    activeFocusOnTab: true

    function toInteger(number) {
        return Math.round(Number(number) * scaleFactor);
    }

    UTSpinBox {
        id: spinBox
        objectName: "spinBox"

        anchors.fill: parent
        focus: true
        from: root.toInteger(root.from)
        to: root.toInteger(root.to)
        value: root.toInteger(root.effectiveValue)
        stepSize: Math.max(1, root.toInteger(root.stepSize))
        editable: true
        validator: DoubleValidator {
            bottom: root.from
            top: root.to
            decimals: root.decimals
            notation: DoubleValidator.StandardNotation
        }
        textFromValue: function(integerValue, locale) {
            return (integerValue / root.scaleFactor).toFixed(root.decimals);
        }
        valueFromText: function(text, locale) {
            const parsed = Number.fromLocaleString(locale, text);
            if (isNaN(parsed))
                return spinBox.value;
            return root.toInteger(Math.max(root.from, Math.min(root.to, parsed)));
        }
        // UTSpinBox's custom TextInput reliably emits valueChanged for wheel,
        // keyboard and accepted text edits; valueModified is not emitted on
        // every one of those paths.  Forward the actual value transition.
        onValueChanged: {
            const next = value / root.scaleFactor;
            if (Math.abs(next - root.effectiveValue) < 0.0000001)
                return;
            root.value = next;
            root.valueModified(next);
        }
    }
}
