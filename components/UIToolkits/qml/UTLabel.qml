/*
 * @file UTLabel.qml
 * @brief UIToolkits 的 UTLabel.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

Label {
    id: root

    enum TextRole {
        H1,
        H2,
        H3,
        H4,
        H5,
        H6,
        Body,
        Caption
    }

    property int textRole: UTLabel.TextRole.Body
    property bool disabled: false
    property bool focusable: false

    // 无障碍
    property string accName: root.text
    property int accRole: Accessible.StaticText
    Accessible.name: accName
    Accessible.role: accRole

    focusPolicy: focusable ? Qt.StrongFocus : Qt.NoFocus
    textFormat: Text.PlainText

    font.pixelSize: {
        switch (textRole) {
        case UTLabel.TextRole.H1:
            return 32;
        case UTLabel.TextRole.H2:
            return 28;
        case UTLabel.TextRole.H3:
            return 24;
        case UTLabel.TextRole.H4:
            return 20;
        case UTLabel.TextRole.H5:
            return 16;
        case UTLabel.TextRole.H6:
            return 14;
        case UTLabel.TextRole.Body:
            return 14;
        case UTLabel.TextRole.Caption:
            return 12;
        default:
            return 14;
        }
    }

    font.weight: {
        switch (textRole) {
        case UTLabel.TextRole.H1:
        case UTLabel.TextRole.H2:
        case UTLabel.TextRole.H3:
        case UTLabel.TextRole.H4:
        case UTLabel.TextRole.H5:
            return Font.Medium;
        case UTLabel.TextRole.H6:
        case UTLabel.TextRole.Body:
            return Font.Normal;
        case UTLabel.TextRole.Caption:
            return Font.Normal;
        default:
            return Font.Normal;
        }
    }

    color: {
        if (disabled)
            return "#bdbdbd";
        if (focusable && activeFocus)
            return (UTTheme.primaryColor);
        return (UTTheme.primaryTextColor);
    }

    opacity: disabled ? 0.6 : 1.0

    Behavior on color {
        ColorAnimation {
            duration: 200
        }
    }
}
