/*
 * @file UTProgressBar.qml
 * @brief UIToolkits 的 UTProgressBar.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

ProgressBar {
    id: root
    property bool showPercent: false
    property color progressColor: UTTheme.primaryColor
    property color trackColor: UTTheme.isDark ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
    property int radius: 8
    property int tooltipPlacement: UTTooltip.Auto

    // 无障碍
    property string accName: ""
    property int accRole: Accessible.ProgressBar
    Accessible.name: accName !== "" ? accName : "Progress " + Math.round(root.position * 100) + "%"
    Accessible.role: accRole

    implicitHeight: 10

    function updateTooltip() {
        const text = Math.round(root.position * 100) + "%";
        const pointerPosition = tooltipHoverHandler.hovered ? tooltipHoverHandler.point.position : Qt.point(-1, -1);
        UTTooltip.update(root, text, root.showPercent && (root.hovered || root.activeFocus), undefined,
                         root.tooltipPlacement, pointerPosition);
    }

    onActiveFocusChanged: root.updateTooltip()
    onHoveredChanged: root.updateTooltip()
    onPositionChanged: root.updateTooltip()
    onShowPercentChanged: root.updateTooltip()
    Component.onDestruction: UTTooltip.hide(root)

    HoverHandler {
        id: tooltipHoverHandler
        onPointChanged: {
            if (root.showPercent && root.hovered
                    && (root.tooltipPlacement === UTTooltip.Auto
                        || root.tooltipPlacement === UTTooltip.FollowMouse))
                UTTooltip.updatePointer(root, root.tooltipPlacement,
                                        tooltipHoverHandler.point.position);
        }
    }

    background: Rectangle {
        implicitHeight: 6
        radius: Math.round((root.radius || UTTheme.radius || 8) * 0.4)
        color: root.trackColor
    }

    contentItem: Item {
        implicitHeight: 6

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: Math.round((root.radius || UTTheme.radius || 8) * 0.4)
            color: root.progressColor

            Behavior on width {
                NumberAnimation {
                    duration: 180
                }
            }
        }
    }

}
