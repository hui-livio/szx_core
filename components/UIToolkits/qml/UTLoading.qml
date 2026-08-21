/*
 * @file UTLoading.qml
 * @brief UIToolkits 的可配置模态加载弹窗组件。
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes

Popup {
    id: root

    enum IndicatorStyle {
        FluentRing,
        WindowsDots
    }

    property alias active: root.visible
    property bool blockInput: true
    property string text: ""
    property color panelColor: UTTheme.isDark ? "#1b222d" : "#ffffff"
    property color borderColor: UTTheme.isDark ? "#3a4656" : "#d8e0eb"
    property bool highlighted: true
    property color highlightColor: UTTheme.primaryColor
    readonly property color effectiveBorderColor: highlighted ? highlightColor : borderColor
    property real panelOpacity: 0.96
    property int indicatorSize: 48
    property int indicatorStyle: UTLoading.FluentRing
    property int dotCount: 8
    property int animationDuration: 1000
    property color indicatorColor: UTTheme.primaryColor
    property color inactiveIndicatorColor: UTTheme.isDark ? "#375786" : "#a8c2ef"
    property int textPixelSize: 16
    property Component contentComponent: null
    default property alias embeddedContent: embeddedContent.data

    implicitWidth: Math.max(120, contentColumn.implicitWidth + leftPadding + rightPadding)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    modal: blockInput
    focus: blockInput
    closePolicy: Popup.NoAutoClose
    padding: 24

    Overlay.modal: Item {}

    background: Rectangle {
        radius: UTTheme.radius * 1.5
        color: root.panelColor
        opacity: Math.max(0, Math.min(1, root.panelOpacity))
        border.width: 1
        border.color: root.effectiveBorderColor
    }

    contentItem: Column {
        id: contentColumn
        spacing: 16

        Item {
            id: fluentSpinner
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.indicatorStyle === UTLoading.FluentRing
            width: root.indicatorSize
            height: root.indicatorSize

            property real sweepAngle: 18
            readonly property real strokeWidth: Math.max(3, root.indicatorSize / 10)
            readonly property real radius: (width - strokeWidth) / 2

            Shape {
                id: fluentArc
                anchors.fill: parent
                antialiasing: true
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: root.indicatorColor
                    strokeWidth: fluentSpinner.strokeWidth
                    capStyle: ShapePath.RoundCap
                    fillColor: "transparent"

                    PathAngleArc {
                        centerX: fluentSpinner.width / 2
                        centerY: fluentSpinner.height / 2
                        radiusX: fluentSpinner.radius
                        radiusY: fluentSpinner.radius
                        startAngle: -90
                        sweepAngle: fluentSpinner.sweepAngle
                    }
                }

                RotationAnimator on rotation {
                    running: root.visible && fluentSpinner.visible
                    from: 0
                    to: 720
                    duration: root.animationDuration * 2
                    loops: Animation.Infinite
                }
            }

            SequentialAnimation on sweepAngle {
                running: root.visible && fluentSpinner.visible
                loops: Animation.Infinite

                NumberAnimation {
                    from: 18
                    to: 292
                    duration: root.animationDuration
                    easing.type: Easing.InOutCubic
                }
                NumberAnimation {
                    from: 292
                    to: 18
                    duration: root.animationDuration
                    easing.type: Easing.InOutCubic
                }
            }
        }

        Item {
            id: windowsSpinner
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.indicatorStyle === UTLoading.WindowsDots
            width: root.indicatorSize
            height: root.indicatorSize

            readonly property int dotCount: Math.max(3, Math.min(8, root.dotCount))
            readonly property real dotSize: Math.max(5, Math.round(root.indicatorSize / 7))
            property real phase: 0

            NumberAnimation on phase {
                running: root.visible && windowsSpinner.visible
                from: 0
                to: windowsSpinner.dotCount
                duration: root.animationDuration
                loops: Animation.Infinite
                easing.type: Easing.Linear
            }

            Repeater {
                model: windowsSpinner.dotCount

                Item {
                    required property int index
                    anchors.fill: parent
                    readonly property real angle: (index / windowsSpinner.dotCount) * Math.PI * 2 - Math.PI / 2
                    readonly property real orbitRadius: (width - windowsSpinner.dotSize) / 2
                    readonly property real rawDistance: Math.abs(index - windowsSpinner.phase)
                    readonly property real distance: Math.min(rawDistance, windowsSpinner.dotCount - rawDistance)
                    readonly property real intensity: Math.max(0, 1 - distance / 2.2)

                    Rectangle {
                        width: windowsSpinner.dotSize
                        height: windowsSpinner.dotSize
                        x: parent.width / 2 + Math.cos(parent.angle) * parent.orbitRadius - width / 2
                        y: parent.height / 2 + Math.sin(parent.angle) * parent.orbitRadius - height / 2
                        radius: width / 2
                        color: Qt.rgba(
                                   root.inactiveIndicatorColor.r
                                   + (root.indicatorColor.r - root.inactiveIndicatorColor.r) * parent.intensity,
                                   root.inactiveIndicatorColor.g
                                   + (root.indicatorColor.g - root.inactiveIndicatorColor.g) * parent.intensity,
                                   root.inactiveIndicatorColor.b
                                   + (root.indicatorColor.b - root.inactiveIndicatorColor.b) * parent.intensity,
                                   1)
                        opacity: 0.2 + parent.intensity * 0.8
                        scale: 0.72 + parent.intensity * 0.38
                    }
                }
            }
        }

        UTLabel {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.text.length > 0
            text: root.text
            color: UTTheme.primaryTextColor
            font.pixelSize: root.textPixelSize
            font.weight: Font.Medium
        }

        Column {
            id: embeddedContent
            spacing: 8
            visible: children.length > 0
        }

        Loader {
            id: componentLoader
            anchors.horizontalCenter: parent.horizontalCenter
            active: root.visible && root.contentComponent !== null
            sourceComponent: root.contentComponent
        }
    }
}
