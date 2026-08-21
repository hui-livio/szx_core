/*
 * @file UTSliderValue.qml
 * @brief UIToolkits 的 UTSliderValue.qml 组件。
 */

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property real from: 0
    property real to: 100
    property real value: 0
    property real stepSize: 1
    property int decimals: 0
    signal valueModified(real value)

    implicitWidth: 150
    implicitHeight: 32

    RowLayout {
        anchors.fill: parent
        spacing: 8

        UTSlider {
            id: slider
            Layout.fillWidth: true
            from: root.from
            to: root.to
            value: root.value
            stepSize: root.stepSize
            onMoved: {
                root.value = value;
                root.valueModified(value);
            }
        }

        UTLabel {
            Layout.preferredWidth: 40
            text: Number(root.value).toFixed(root.decimals)
            horizontalAlignment: Text.AlignRight
        }
    }
}
