/*
 * @file UTButton.qml
 * @brief UIToolkits 的 UTButton.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

Button {
    id: root

    enum ButtonType {
        Filled,
        Tonal,
        Outlined,
        Text,
        Surface
    }

    property int buttonType: UTButton.ButtonType.Filled
    property color primaryColor: UTTheme.primaryColor
    property color primaryForegroundColor: UTTheme.primaryTextColor
    property color pressedColor: Qt.darker(primaryColor, 1.15)
    property color tonalColor: Qt.darker(primaryColor, isDarkTheme ? 1.55 : 1.12)
    readonly property color tonalForegroundColor: isDarkTheme ? primaryForegroundColor : "white"
    property color hoverColor: UTTheme.backgroundColor
    property color disabledColor: "#bdbdbd"

    readonly property bool isDarkTheme: (UTTheme.isDark === true)

    // Hover 颜色：使用焦点框颜色（primaryColor）的透明版本
    property color surfaceButtonHoverColor: Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.10)
    property color surfaceButtonPressedColor: Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.16)
    property color filledButtonHoverColor: Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.12)
    property color filledButtonPressedColor: Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.18)
    property color tonalButtonHoverColor: Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.08)
    property color tonalButtonPressedColor: Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.14)
    property color textButtonHoverColor: Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.10)
    property color textButtonPressedColor: Qt.rgba(root.primaryColor.r, root.primaryColor.g, root.primaryColor.b, 0.18)
    property string tooltipText: ""
    property int tooltipPlacement: UTTooltip.Auto
    // Allow compact dialogs to opt out of the default ellipsis behavior.
    property int textElide: Text.ElideRight

    // Hover 边框颜色：焦点框颜色的透明版本
    property color surfaceButtonHoverBorderColor: UTTheme.surfaceButtonHoverBorderColor

    // 无障碍
    property string accName: root.text
    property int accRole: Accessible.Button
    Accessible.name: accName
    Accessible.role: accRole

    implicitWidth: Math.max(88, contentItem.implicitWidth + 24)
    implicitHeight: 36
    font.pixelSize: 14
    font.weight: 500
    spacing: 8
    padding: 12
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

    contentItem: Text {
        id: contentText
        text: root.text
        color: {
            if (!root.enabled) {
                return root.disabledColor;
            }
            switch (root.buttonType) {
            case UTButton.ButtonType.Filled:
            case UTButton.ButtonType.Surface:
                return root.primaryForegroundColor;
            case UTButton.ButtonType.Tonal:
                return root.tonalForegroundColor;
            case UTButton.ButtonType.Outlined:
                return root.primaryColor;
            case UTButton.ButtonType.Text:
                return (root.hovered || root.down) ? Qt.lighter(root.primaryColor, 1.15) : root.primaryColor;
            }
        }
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: root.textElide

        scale: (root.buttonType === UTButton.ButtonType.Text && root.down) ? 1.15 : 1.0

        Behavior on scale {
            NumberAnimation {
                duration: 150
            }
        }
    }

    background: Item {
        id: bgItem

        Rectangle {
            anchors.fill: parent
            radius: UTTheme.radius
            color: {
                if (!root.enabled) {
                    return root.hoverColor;
                }
                if (root.down) {
                    switch (root.buttonType) {
                    case UTButton.ButtonType.Filled:
                        return root.pressedColor;
                    case UTButton.ButtonType.Surface:
                        return Qt.darker(UTTheme.surfaceColor, 1.1);
                    case UTButton.ButtonType.Tonal:
                        return Qt.darker(root.tonalColor, 1.2);
                    case UTButton.ButtonType.Outlined:
                    case UTButton.ButtonType.Text:
                        return "transparent";
                    }
                }
                switch (root.buttonType) {
                case UTButton.ButtonType.Filled:
                    return root.primaryColor;
                case UTButton.ButtonType.Surface:
                    return UTTheme.surfaceColor;
                case UTButton.ButtonType.Tonal:
                    return root.tonalColor;
                case UTButton.ButtonType.Outlined:
                case UTButton.ButtonType.Text:
                    return "transparent";
                }
            }
            border.width: {
                if (root.hovered && root.buttonType === UTButton.ButtonType.Filled && root.enabled)
                    return 1.5;
                if (root.hovered && root.buttonType === UTButton.ButtonType.Surface && root.enabled)
                    return 1.5;
                if (root.buttonType === UTButton.ButtonType.Outlined && root.enabled)
                    return 1;
                if (root.buttonType === UTButton.ButtonType.Surface && root.enabled)
                    return 1;
                return 0;
            }
            border.color: {
                if (root.hovered && root.buttonType === UTButton.ButtonType.Filled) {
                    return isDarkTheme ? Qt.rgba(1, 1, 1, 0.40) : Qt.rgba(1, 1, 1, 0.50);
                }
                if (root.hovered && root.buttonType === UTButton.ButtonType.Surface) {
                    return root.surfaceButtonHoverBorderColor;
                }
                if (root.buttonType === UTButton.ButtonType.Surface)
                    return UTTheme.borderColor;
                return root.primaryColor;
            }

            Behavior on color {
                ColorAnimation {
                    duration: 200
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

        Rectangle {
            anchors.fill: parent
            radius: UTTheme.radius || 8
            visible: root.enabled && (root.hovered || root.down)
            color: {
                if (!root.enabled)
                    return "transparent";

                if (root.down) {
                    switch (root.buttonType) {
                    case UTButton.ButtonType.Filled:
                        return root.filledButtonPressedColor;
                    case UTButton.ButtonType.Surface:
                        return root.surfaceButtonPressedColor;
                    case UTButton.ButtonType.Tonal:
                        return root.tonalButtonPressedColor;
                    case UTButton.ButtonType.Outlined:
                        return root.textButtonPressedColor;
                    case UTButton.ButtonType.Text:
                        return "transparent";
                    default:
                        return "transparent";
                    }
                }

                if (root.hovered) {
                    switch (root.buttonType) {
                    case UTButton.ButtonType.Filled:
                        return root.filledButtonHoverColor;
                    case UTButton.ButtonType.Surface:
                        return root.surfaceButtonHoverColor;
                    case UTButton.ButtonType.Tonal:
                        return root.tonalButtonHoverColor;
                    case UTButton.ButtonType.Outlined:
                        return root.textButtonHoverColor;
                    case UTButton.ButtonType.Text:
                        return "transparent";
                    default:
                        return "transparent";
                    }
                }

                return "transparent";
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                }
            }

            Behavior on color {
                ColorAnimation {
                    duration: 100
                }
            }

            opacity: (root.hovered || root.down) ? 1.0 : 0.0
        }
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
}
