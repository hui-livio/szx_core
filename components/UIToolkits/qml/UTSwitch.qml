/*
 * @file UTSwitch.qml
 * @brief UIToolkits 的 UTSwitch.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

Switch {
    id: root

    spacing: 12
    font.pixelSize: 14
    leftPadding: 4
    topPadding: 4
    bottomPadding: 4
    property string tooltipText: ""
    property int tooltipPlacement: UTTooltip.Auto

    // 无障碍
    property string accName: root.text
    property int accRole: Accessible.CheckBox
    Accessible.name: accName
    Accessible.role: accRole

    activeFocusOnTab: true

    function updateTooltip() {
        const pointerPosition = tooltipHoverHandler.hovered
                              ? tooltipHoverHandler.point.position
                              : Qt.point(-1, -1);
        UTTooltip.update(root, root.tooltipText,
                         root.hovered || root.activeFocus,
                         undefined, root.tooltipPlacement, pointerPosition);
    }

    onActiveFocusChanged: root.updateTooltip()
    onHoveredChanged: root.updateTooltip()
    onTooltipPlacementChanged: root.updateTooltip()
    onTooltipTextChanged: root.updateTooltip()
    Component.onDestruction: UTTooltip.hide(root)

    indicator: Rectangle {
        implicitWidth: 48
        implicitHeight: 28
        x: root.leftPadding
        y: root.height / 2 - height / 2
        radius: height / 2
        color: root.checked ? (UTTheme.primaryColor) : (root.enabled ? "#e0e0e0" : "#bdbdbd")
        border.width: root.activeFocus ? 2 : 0
        border.color: root.activeFocus ? (UTTheme.primaryColor) : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: 300
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

        Rectangle {
            width: 24
            height: 24
            radius: Math.round((UTTheme.radius || 8) * 1.5)
            color: "#ffffff"
            x: root.checked ? parent.width - width - 2 : 2
            y: (parent.height - height) / 2

            Behavior on x {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.InOutQuad
                }
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#0000002d"
                shadowVerticalOffset: 2
                shadowBlur: 0.4
            }
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
        id: tooltipHoverHandler

        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPointChanged: {
            if (root.hovered
                    && (root.tooltipPlacement === UTTooltip.Auto
                        || root.tooltipPlacement === UTTooltip.FollowMouse))
                UTTooltip.updatePointer(root, root.tooltipPlacement,
                                        tooltipHoverHandler.point.position);
        }
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
