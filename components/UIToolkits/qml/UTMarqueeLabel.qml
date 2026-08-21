/*
 * @file UTMarqueeLabel.qml
 * @brief UIToolkits 的主题化滚动字幕控件。
 */

import QtQuick

Item {
    id: root

    property alias text: textItem.text
    property color textColor: UTTheme.primaryTextColor
    property font textFont: Qt.font({ family: UTTheme.fontFamily, pixelSize: 14 })
    property int textAlignment: Text.AlignLeft
    property bool running: true
    property bool pauseOnHover: true
    property real pixelsPerSecond: 42
    property int restartDelay: 600
    property int loopDelay: 900
    property int sidePadding: 4

    readonly property bool overflowing: textItem.implicitWidth > Math.max(0, root.width - root.sidePadding * 2)
    readonly property real travelDistance: textItem.implicitWidth + root.width + root.sidePadding * 2

    implicitWidth: 200
    implicitHeight: Math.max(28, textItem.implicitHeight + 8)
    clip: true

    Accessible.name: root.text
    Accessible.role: Accessible.StaticText

    function staticTextX() {
        const availableWidth = Math.max(0, root.width - root.sidePadding * 2);
        if (root.textAlignment === Text.AlignRight)
            return root.width - root.sidePadding - textItem.implicitWidth;
        if (root.textAlignment === Text.AlignHCenter)
            return root.sidePadding + Math.max(0, (availableWidth - textItem.implicitWidth) / 2);
        return root.sidePadding;
    }

    Text {
        id: textItem

        width: implicitWidth
        y: Math.max(0, (root.height - implicitHeight) / 2)
        x: root.overflowing ? root.width : root.staticTextX()
        color: root.textColor
        font: root.textFont
        textFormat: Text.PlainText
        elide: Text.ElideNone
        verticalAlignment: Text.AlignVCenter

        SequentialAnimation on x {
            id: scrollAnimation
            running: root.running && root.overflowing && !hoverHandler.hovered
            loops: Animation.Infinite

            PauseAnimation {
                duration: root.restartDelay
            }
            NumberAnimation {
                from: root.width
                to: -textItem.width - root.sidePadding
                duration: Math.max(800, root.travelDistance / Math.max(1, root.pixelsPerSecond) * 1000)
                easing.type: Easing.Linear
            }
            PauseAnimation {
                duration: root.loopDelay
            }
        }

        HoverHandler {
            id: hoverHandler
            enabled: root.pauseOnHover
        }

        Connections {
            target: root
            function onTextChanged() {
                scrollAnimation.restart();
            }
            function onWidthChanged() {
                scrollAnimation.restart();
            }
            function onOverflowingChanged() {
                scrollAnimation.restart();
            }
        }
    }
}
