/*
 * @file UTCard.qml
 * @brief UIToolkits 的 UTCard.qml 组件。
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: root

    // ── 信号 ────────────────────────────────────────────────────
    signal clicked
    signal doubleClicked

    // ── 主题 ────────────────────────────────────────────────────

    // ── 内容属性 ────────────────────────────────────────────────
    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text
    property bool showSubtitle: true

    // ── Avatar ──────────────────────────────────────────────────
    property url avatarSource: ""
    property int avatarSize: 40

    // ── 样式 ────────────────────────────────────────────────────
    property bool selected: false
    property real radius: UTTheme.radius || 8
    property int padding: 10
    property real pressedScale: 0.97
    property bool hoverEnabled: true

    // ── 阴影 ────────────────────────────────────────────────────
    property int elevation: 1
    property bool shadowEnabled: elevation > 0

    // ── 无障碍 ──────────────────────────────────────────────────
    property string accName: title
    property int accRole: selected ? Accessible.Button : Accessible.StaticText
    Accessible.name: accName
    Accessible.role: accRole

    // ── 自适应尺寸 ──────────────────────────────────────────────
    implicitWidth: 300
    implicitHeight: contentLayout.implicitHeight + padding * 2

    color: "transparent"

    // ── 缩放变换 ────────────────────────────────────────────────
    transform: Scale {
        id: scaleTransform
        origin.x: root.width / 2
        origin.y: root.height / 2
    }

    // ── 背景 + 阴影 ─────────────────────────────────────────────
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.radius

        color: {
            if (root.selected)
                return UTTheme.primaryColor ? Qt.rgba(UTTheme.primaryColor.r, UTTheme.primaryColor.g, UTTheme.primaryColor.b, 0.08) : "#0a3a6df0";
            if (root.hoverEnabled && mouseArea.containsMouse)
                return UTTheme.surfaceColor ? Qt.lighter(UTTheme.surfaceColor, 1.04) : "#f8f9fa";
            return UTTheme.surfaceColor;
        }

        border.color: {
            if (root.selected)
                return UTTheme.primaryColor;
            if (root.hoverEnabled && (mouseArea.containsMouse || root.activeFocus))
                return UTTheme.primaryColor;
            return UTTheme.borderColor;
        }
        border.width: (root.selected || (root.hoverEnabled && mouseArea.containsMouse) || root.activeFocus) ? 2 : 1

        Behavior on color {
            ColorAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 200
            }
        }
        Behavior on border.width {
            NumberAnimation {
                duration: 160
            }
        }
    }

    // ── 阴影效果 ────────────────────────────────────────────────
    MultiEffect {
        source: bg
        anchors.fill: bg
        visible: root.shadowEnabled
        shadowEnabled: true
        shadowColor: UTTheme.shadowColor
        shadowBlur: root.elevation * 4
        shadowVerticalOffset: root.elevation * 1.5
        shadowHorizontalOffset: 0
    }

    // ── 内容布局 ────────────────────────────────────────────────
    RowLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: root.padding
        spacing: 12

        // avatar
        Rectangle {
            id: avatarWrap
            Layout.preferredWidth: avatarImage.status === Image.Ready ? root.avatarSize : 0
            Layout.preferredHeight: root.avatarSize
            Layout.alignment: Qt.AlignVCenter
            visible: avatarImage.status === Image.Ready
            radius: Math.round(root.radius * 0.5)
            color: "transparent"
            clip: true

            Image {
                id: avatarImage
                anchors.fill: parent
                source: root.avatarSource
                sourceSize: Qt.size(root.avatarSize * 2, root.avatarSize * 2)
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                smooth: true
            }
        }

        ColumnLayout {
            id: textCol
            Layout.fillWidth: true
            spacing: 4

            Label {
                id: titleLabel
                Layout.fillWidth: true
                font.pixelSize: 16
                font.weight: Font.Medium
                color: root.selected ? (UTTheme.primaryColor) : (UTTheme.primaryTextColor)
                visible: text !== ""
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: 2
            }

            Label {
                id: subtitleLabel
                Layout.fillWidth: true
                font.pixelSize: 13
                color: UTTheme.secondaryTextColor
                visible: root.showSubtitle && text !== ""
                wrapMode: Text.WordWrap
            }
        }
    }

    // ── 交互 ────────────────────────────────────────────────────
    // ⚠ MouseArea 必须放在最后，确保在所有内容之上接收事件
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.hoverEnabled
        cursorShape: root.hoverEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor

        onPressed: {
            scaleTransform.xScale = root.pressedScale;
            scaleTransform.yScale = root.pressedScale;
            bg.opacity = 0.85;
        }

        onReleased: {
            restoreAnimation.restart();
            bg.opacity = 1.0;
            root.clicked();
        }

        onCanceled: {
            restoreAnimation.restart();
            bg.opacity = 1.0;
        }

        onDoubleClicked: {
            root.doubleClicked();
        }
    }

    // ── 恢复动画 ────────────────────────────────────────────────
    ParallelAnimation {
        id: restoreAnimation
        SpringAnimation {
            target: scaleTransform
            property: "xScale"
            to: 1.0
            spring: 2.5
            damping: 0.25
        }
        SpringAnimation {
            target: scaleTransform
            property: "yScale"
            to: 1.0
            spring: 2.5
            damping: 0.25
        }
    }
}
