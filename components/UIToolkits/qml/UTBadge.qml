/*
 * @file UTBadge.qml
 * @brief UIToolkits 的 UTBadge.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

Label {
    id: root

    enum Tone {
        Neutral,
        Info,
        Success,
        Warning,
        Danger
    }

    property int tone: UTBadge.Tone.Neutral

    readonly property color toneColor: {
        switch (tone) {
        case UTBadge.Tone.Info: return UTTheme.primaryColor;
        case UTBadge.Tone.Success: return "#22c55e";
        case UTBadge.Tone.Warning: return "#f59e0b";
        case UTBadge.Tone.Danger: return "#ef4444";
        default: return UTTheme.secondaryTextColor;
        }
    }

    height: 28
    leftPadding: 10
    rightPadding: 10
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    color: toneColor

    background: Rectangle {
        radius: height / 2
        color: Qt.rgba(root.toneColor.r, root.toneColor.g, root.toneColor.b, 0.14)
        border.width: 1
        border.color: Qt.rgba(root.toneColor.r, root.toneColor.g, root.toneColor.b, 0.45)
    }
}
