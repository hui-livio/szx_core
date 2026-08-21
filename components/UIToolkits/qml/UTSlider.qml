/*
 * @file UTSlider.qml
 * @brief UIToolkits 的 UTSlider.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

Slider {
    id: root

    property bool showValue: false
    property string valueSuffix: ""
    property int tooltipPlacement: UTTooltip.Auto

    // 无障碍
    property string accName: ""
    property int accRole: Accessible.Slider
    Accessible.name: accName !== "" ? accName : "Slider " + Math.round(root.value)
    Accessible.role: accRole

    activeFocusOnTab: true
    property color activeColor: UTTheme.primaryColor
    property color inactiveColor: UTTheme.isDark ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
    property color thumbColor: "#ffffff"

    implicitHeight: 30

    function updateTooltip() {
        const text = Number(root.value).toFixed(0) + root.valueSuffix;
        const pointerPosition = tooltipHoverHandler.hovered ? tooltipHoverHandler.point.position : Qt.point(-1, -1);
        UTTooltip.update(root, text, root.showValue && (root.pressed || root.hovered || root.activeFocus), undefined,
                         root.tooltipPlacement, pointerPosition);
    }

    onActiveFocusChanged: root.updateTooltip()
    onHoveredChanged: root.updateTooltip()
    onPressedChanged: root.updateTooltip()
    onShowValueChanged: root.updateTooltip()
    onValueChanged: root.updateTooltip()
    onValueSuffixChanged: root.updateTooltip()
    Component.onDestruction: UTTooltip.hide(root)

    HoverHandler {
        id: tooltipHoverHandler
        onPointChanged: {
            if (root.showValue && root.hovered
                    && (root.tooltipPlacement === UTTooltip.Auto
                        || root.tooltipPlacement === UTTooltip.FollowMouse))
                UTTooltip.updatePointer(root, root.tooltipPlacement,
                                        tooltipHoverHandler.point.position);
        }
    }

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 6
        radius: Math.round((UTTheme.radius || 8) * 0.4)
        color: root.inactiveColor

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: root.activeColor
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: root.hovered ? 24 : 20
        implicitHeight: root.hovered ? 24 : 20
        // 悬停放大时保持正圆，避免出现圆角方块。
        radius: width / 2
        color: root.enabled ? root.thumbColor : "#dddddd"
        border.width: root.hovered ? 3 : 2
        border.color: root.activeColor

        Behavior on implicitWidth {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
        Behavior on implicitHeight {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
        Behavior on border.width {
            NumberAnimation {
                duration: 120
            }
        }

        Behavior on x {
            NumberAnimation {
                duration: 120
            }
        }
    }

}
