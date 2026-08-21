/*
 * @file UTFootBar.qml
 * @brief UIToolkits 的标准窗口底部信息栏组件。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Control {
    id: root

    enum StatusTone {
        Neutral,
        Info,
        Success,
        Warning,
        Danger
    }

    enum StatusIndicatorPosition {
        Left,
        Center
    }

    property string statusText: ""
    property string centerText: ""
    property string rightText: ""
    property string rightToolTip: ""
    property bool rightToolTipPinned: false
    readonly property real rightToolTipMaximumWidth: Math.min(600,
                                                               root.Window.window
                                                               ? root.Window.window.width * 0.6 : 600)
    readonly property real rightToolTipMaximumHeight: Math.min(320,
                                                                root.Window.window
                                                                ? root.Window.window.height * 0.5 : 320)
    property int statusTone: UTFootBar.StatusTone.Neutral
    property int statusIndicatorPosition: UTFootBar.StatusIndicatorPosition.Left
    property bool showStatusIndicator: statusText.length > 0

    property color backgroundColor: UTTheme.surfaceColor
    property color borderColor: UTTheme.borderColor
    property color textColor: UTTheme.primaryTextColor
    property color secondaryTextColor: UTTheme.secondaryTextColor
    property color rightTextColor: root.secondaryTextColor
    property color statusColor: {
        switch (statusTone) {
        case UTFootBar.StatusTone.Info:
            return UTTheme.infoColor || "#38bdf8";
        case UTFootBar.StatusTone.Success:
            return UTTheme.successColor || "#22c55e";
        case UTFootBar.StatusTone.Warning:
            return UTTheme.warningColor || "#f59e0b";
        case UTFootBar.StatusTone.Danger:
            return UTTheme.errorColor || "#ef4444";
        default:
            return UTTheme.secondaryTextColor;
        }
    }

    property alias leftContent: leftSlot.data
    property alias centerContent: centerSlot.data
    property alias rightContent: rightSlot.data

    implicitWidth: 960
    implicitHeight: 32
    leftPadding: 12
    rightPadding: 12
    topPadding: 1
    bottomPadding: 0
    spacing: 12

    Accessible.role: Accessible.StatusBar
    Accessible.name: statusText.length > 0 ? statusText : qsTr("Status bar")

    onRightToolTipChanged: {
        if (root.rightToolTip.length === 0)
            root.rightToolTipPinned = false;
    }

    background: Rectangle {
        color: root.backgroundColor

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: root.borderColor
        }
    }

    contentItem: RowLayout {
        spacing: root.spacing

        RowLayout {
            id: leftSlot
            Layout.minimumWidth: 0
            Layout.preferredWidth: Math.min(implicitWidth, root.availableWidth * 0.55)
            Layout.maximumWidth: root.availableWidth * 0.55
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            spacing: 8
            visible: root.statusText.length > 0

            Rectangle {
                objectName: "statusIndicator"
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: root.statusColor
                visible: root.showStatusIndicator
                         && root.statusIndicatorPosition === UTFootBar.StatusIndicatorPosition.Left
                Accessible.ignored: true
            }

            Text {
                objectName: "statusLabel"
                text: root.statusText
                color: root.textColor
                font.family: UTTheme.fontFamily
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                visible: text.length > 0
            }
        }

        RowLayout {
            id: centerSlot
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            spacing: 8
            visible: root.centerText.length > 0

            Rectangle {
                objectName: "centerStatusIndicator"
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: root.statusColor
                visible: root.showStatusIndicator
                         && root.statusIndicatorPosition === UTFootBar.StatusIndicatorPosition.Center
                Accessible.ignored: true
            }

            Text {
                objectName: "centerLabel"
                text: root.centerText
                color: root.secondaryTextColor
                font.family: UTTheme.fontFamily
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                visible: text.length > 0
            }
        }

        Item {
            id: trailingSpacer
            objectName: "trailingSpacer"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: !centerSlot.visible
        }

        RowLayout {
            id: rightSlot
            objectName: "rightSlot"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            spacing: 8
            visible: root.rightText.length > 0
            activeFocusOnTab: root.rightToolTip.length > 0

            Accessible.role: Accessible.StaticText
            Accessible.name: root.rightText
            Accessible.description: root.rightToolTip

            Keys.onEscapePressed: root.rightToolTipPinned = false
            Keys.onSpacePressed: root.rightToolTipPinned = !root.rightToolTipPinned

            HoverHandler {
                id: rightHoverHandler
            }

            TapHandler {
                onTapped: root.rightToolTipPinned = !root.rightToolTipPinned
            }

            ToolTip.visible: root.rightToolTip.length > 0
                             && (rightHoverHandler.hovered || rightSlot.activeFocus || root.rightToolTipPinned)
            ToolTip.text: root.rightToolTip
            ToolTip.delay: 400
            ToolTip.toolTip.objectName: "rightToolTipPopup"
            ToolTip.toolTip.width: Math.min(Math.max(240, ToolTip.toolTip.implicitWidth), root.rightToolTipMaximumWidth)
            ToolTip.toolTip.height: Math.min(ToolTip.toolTip.implicitHeight, root.rightToolTipMaximumHeight)

            Text {
                objectName: "rightLabel"
                text: root.rightText
                color: root.rightTextColor
                font.family: UTTheme.fontFamily
                font.pixelSize: 12
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                visible: text.length > 0
            }
        }
    }
}
