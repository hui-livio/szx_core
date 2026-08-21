/*
 * @file DemoSelections.qml
 * @brief UIToolkits 展示应用的 DemoSelections.qml 页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null

    readonly property var theme: UTTheme.theme

    property real demoProgress: 0.35

    Timer {
        interval: 110
        running: true
        repeat: true
        onTriggered: {
            root.demoProgress += 0.01;
            if (root.demoProgress > 1.0)
                root.demoProgress = 0.0;
        }
    }

    contentWidth: contentCol.width
    contentHeight: contentCol.implicitHeight

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 24

        UTLabel {
            text: "Selection Controls"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "CheckBox"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        ColumnLayout {
            spacing: 12
            Layout.fillWidth: true
            UTCheckBox {
                text: "Option 1"
                checked: true
            }
            UTCheckBox {
                text: "Option 2"
                checked: false
            }
            UTCheckBox {
                text: "Disabled option"
                checked: true
                enabled: false
            }
        }

        UTLabel {
            text: "Switch"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        ColumnLayout {
            spacing: 12
            Layout.fillWidth: true
            UTSwitch {
                text: "Feature enabled"
                checked: true
            }
            UTSwitch {
                text: "Feature disabled"
                checked: false
            }
            UTSwitch {
                text: "Disabled switch"
                enabled: false
            }
        }

        UTLabel {
            text: "Slider & Progress"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            UTSlider {
                id: sliderDemo
                Layout.fillWidth: true
                from: 0
                to: 100
                value: 42
                showValue: true
                valueSuffix: "%"
            }

            UTProgressBar {
                Layout.fillWidth: true
                value: sliderDemo.value
                from: sliderDemo.from
                to: sliderDemo.to
                showPercent: true
            }

            UTProgressBar {
                Layout.fillWidth: true
                value: root.demoProgress
                from: 0
                to: 1
            }
        }

        // ── 双滑块区间选择器 ─────────────────────────────────────
        UTLabel {
            text: "Range Slider"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            UTRangeSlider {
                id: rangeSlider1
                Layout.fillWidth: true
                from: 0
                to: 100
                minValue: 25
                maxValue: 75
                stepSize: 1
                showValue: true
                valueSuffix: "%"
            }

            UTLabel {
                text: "Side values (editable)"
                textRole: UTLabel.TextRole.Body
                color: theme.secondaryTextColor
            }

            UTRangeSlider {
                id: rangeSliderSideValues
                Layout.fillWidth: true
                from: 0
                to: 1
                minValue: 0.2
                maxValue: 0.8
                stepSize: 0.05
                showValue: true
                valuePrecision: 2
                valueDisplayMode: UTRangeSlider.SideValues
            }

            UTLabel {
                text: "Range: " + Number(rangeSlider1.minValue).toFixed(0) + "% - " + Number(rangeSlider1.maxValue).toFixed(0) + "%"
                textRole: UTLabel.TextRole.Body
                color: theme.secondaryTextColor
            }

            UTLabel {
                text: "Editable range: " + Number(rangeSliderSideValues.minValue).toFixed(2)
                      + " - " + Number(rangeSliderSideValues.maxValue).toFixed(2)
                textRole: UTLabel.TextRole.Body
                color: theme.secondaryTextColor
            }

            UTLabel {
                text: "8-bit integer range (editable)"
                textRole: UTLabel.TextRole.Body
                color: theme.secondaryTextColor
            }

            UTRangeSlider {
                id: rangeSliderByteValues
                Layout.fillWidth: true
                from: 0
                to: 255
                minValue: 32
                maxValue: 224
                stepSize: 1
                showValue: true
                valueDisplayMode: UTRangeSlider.SideValues
            }

            UTLabel {
                text: "Byte range: " + rangeSliderByteValues.minValue + " - " + rangeSliderByteValues.maxValue
                textRole: UTLabel.TextRole.Body
                color: theme.secondaryTextColor
            }
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }
}
