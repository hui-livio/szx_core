/*
 * @file UTSvgButton.qml
 * @brief UIToolkits 的 UTSvgButton.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

Button {
    id: root
    property url svgSource: ""
    property size svgSize: Qt.size(18, 18)
    property color svgColor: "#FFFFFF"
    property color bgHoverColor: Qt.rgba(1, 1, 1, 0.08)
    property color bgPressedColor: Qt.rgba(1, 1, 1, 0.15)
    property string tooltipText: ""
    property int tooltipPlacement: UTTooltip.Auto

    // 无障碍
    property string accName: root.tooltipText
    property int accRole: Accessible.Button
    Accessible.name: accName
    Accessible.role: accRole

    flat: true
    display: AbstractButton.IconOnly

    function updateTooltip() {
        const pointerPosition = tooltipHoverHandler.hovered ? tooltipHoverHandler.point.position : Qt.point(-1, -1);
        UTTooltip.update(root, root.tooltipText, root.hovered || root.activeFocus, undefined, root.tooltipPlacement,
                         pointerPosition);
    }

    onActiveFocusChanged: root.updateTooltip()
    onHoveredChanged: root.updateTooltip()
    onTooltipTextChanged: root.updateTooltip()
    Component.onDestruction: UTTooltip.hide(root)

    HoverHandler {
        id: tooltipHoverHandler
        onPointChanged: {
            if (root.hovered
                    && (root.tooltipPlacement === UTTooltip.Auto
                        || root.tooltipPlacement === UTTooltip.FollowMouse))
                UTTooltip.updatePointer(root, root.tooltipPlacement,
                                        tooltipHoverHandler.point.position);
        }
    }

    icon.source: root.svgSource
    icon.color: root.svgColor
    icon.width: root.svgSize.width
    icon.height: root.svgSize.height

    background: Rectangle {
        radius: Math.round((UTTheme.radius || 8) * 0.5)
        color: root.down ? root.bgPressedColor : root.hovered ? root.bgHoverColor : "transparent"
        border.width: root.activeFocus ? 2 : 0
        border.color: root.activeFocus ? (UTTheme.primaryColor) : "transparent"
        Behavior on border.color {
            ColorAnimation {
                duration: 200
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: 100
            }
        }
    }

}
