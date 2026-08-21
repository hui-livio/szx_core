/*
 * @file UTRoundButton.qml
 * @brief UIToolkits 的 UTRoundButton.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

RoundButton {
    id: root

    enum ButtonVariant {
        Filled,
        Tonal,
        Outlined,
        Text
    }

    property int variant: UTRoundButton.ButtonVariant.Filled
    property color primaryColor: UTTheme.primaryColor
    property color primaryForegroundColor: UTTheme.primaryTextColor
    property color disabledColor: "#bdbdbd"
    property color disabledBgColor: "#eeeeee"

    // Convenience: size property sets both width and height uniformly
    property int buttonSize: 48

    // SVG / image icon support
    // Set iconSource to a URL such as "qrc:/icons/add.svg" or "file:///path/to/icon.svg"
    // iconSize defaults to 60% of buttonSize; set explicitly to override
    property url iconSource: ""
    property int iconSize: Math.round(buttonSize * 0.6)

    // 无障碍
    property string accName: root.text
    property int accRole: Accessible.Button
    Accessible.name: accName
    Accessible.role: accRole

    implicitWidth: buttonSize
    implicitHeight: buttonSize
    radius: buttonSize / 2

    font.pixelSize: 14
    font.weight: 500
    padding: 0

    // Computed icon tint color (reused by both contentItem children)
    readonly property color _iconColor: {
        if (!root.enabled)
            return root.disabledColor;
        switch (root.variant) {
        case UTRoundButton.ButtonVariant.Filled:
            return root.primaryForegroundColor;
        default:
            return root.primaryColor;
        }
    }

    contentItem: Item {
        // Text label — visible only when no icon is set
        Text {
            visible: root.iconSource.toString() === ""
            anchors.centerIn: parent
            text: root.text
            font: root.font
            color: root._iconColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            // 点击时文本下沉效果
            y: root.down ? 1 : 0

            Behavior on y {
                NumberAnimation {
                    duration: 100
                }
            }
        }

        // SVG / raster icon — visible only when iconSource is set
        Item {
            id: iconContainer
            visible: root.iconSource.toString() !== ""
            anchors.centerIn: parent
            width: root.iconSize
            height: root.iconSize

            // 点击时图标下沉效果
            y: root.down ? 1 : 0

            Behavior on y {
                NumberAnimation {
                    duration: 100
                }
            }

            Image {
                id: iconImage
                anchors.fill: parent
                source: root.iconSource
                sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: false
            }

            // Colorize the SVG/image to match the button foreground color
            MultiEffect {
                anchors.fill: iconImage
                source: iconImage
                visible: iconImage.status === Image.Ready
                colorization: 1.0
                colorizationColor: root._iconColor
            }
        }
    }

    background: Rectangle {
        radius: root.radius
        color: {
            if (!root.enabled) {
                return "transparent";
            }
            if (root.down) {
                return Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.12);
            }
            if (root.hovered) {
                return Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.08);
            }
            return "transparent";
        }
        border.width: {
            if (root.activeFocus)
                return 2;
            if (root.hovered && root.enabled)
                return 2;
            return 1;
        }
        border.color: {
            if (!root.enabled)
                return root.disabledColor;
            if (root.hovered)
                return root.primaryColor;
            return Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.3);
        }

        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }

        Behavior on border.color {
            ColorAnimation {
                duration: 150
            }
        }

        Behavior on border.width {
            NumberAnimation {
                duration: 100
            }
        }
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
