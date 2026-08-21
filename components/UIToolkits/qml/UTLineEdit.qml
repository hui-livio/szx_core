/*
 * @file UTLineEdit.qml
 * @brief UIToolkits 的 UTLineEdit.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: root
    property string helperText: ""
    property string errorText: ""
    property bool isError: errorText !== ""
    property int prefixTextWidth: 0
    property string prefixText: ""

    // 无障碍
    property string accName: root.placeholderText || root.text
    property int accRole: Accessible.EditableText
    Accessible.name: accName
    Accessible.role: accRole

    // ── password mode ──────────────────────────────────────────────
    property bool passwordMode: false
    property bool passwordVisible: false

    // ── theme-aware colors (palette 提供完整色值，无需 luminance 计算) ─
    property color primaryColor: UTTheme.primaryColor
    property color textColor: UTTheme.primaryTextColor
    property color placeholderColor: UTTheme.secondaryTextColor
    property color fieldBg: UTTheme.surfaceColor
    property alias backgroundColor: root.fieldBg
    property color fieldBgDisabled: {
        var c = fieldBg;
        return Qt.rgba(c.r * 0.92, c.g * 0.92, c.b * 0.92, c.a);
    }
    property color borderNormal: UTTheme.borderColor
    property color borderHover: primaryColor
    property color borderFocus: primaryColor
    property color borderError: "#f44336"
    property color borderDisabled: {
        var c = borderNormal;
        return Qt.rgba(c.r, c.g, c.b, c.a * 0.4);
    }

    implicitWidth: 280
    implicitHeight: 40
    leftPadding: 16
    property bool hasClearButton: root.text.length > 0
    readonly property int clearButtonWidth: 28
    readonly property int passwordButtonWidth: 28
    rightPadding: {
        var pad = 12;
        if (hasClearButton)
            pad += clearButtonWidth;
        if (passwordMode)
            pad += passwordButtonWidth;
        return pad;
    }
    topPadding: 8
    bottomPadding: 8
    font.pixelSize: 14
    color: isError ? borderError : root.textColor
    placeholderTextColor: root.placeholderColor
    selectByMouse: true
    echoMode: (passwordMode && !passwordVisible) ? TextInput.Password : TextInput.Normal

    background: Rectangle {
        id: bgRect
        radius: UTTheme.radius || 8
        color: root.enabled ? root.fieldBg : root.fieldBgDisabled

        HoverHandler {
            id: hoverHandler
        }

        border.width: root.activeFocus ? 2 : 1
        border.color: {
            if (!root.enabled)
                return root.borderDisabled;
            if (root.isError)
                return root.borderError;
            if (root.activeFocus)
                return root.borderFocus;
            if (hoverHandler.hovered)
                return root.borderHover;
            return root.borderNormal;
        }

        Behavior on border.width {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 200
            }
        }
    }

    // ── prefix text ────────────────────────────────────────────────
    Row {
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
            leftMargin: 12
        }
        spacing: 8
        visible: root.prefixText !== ""

        Text {
            text: root.prefixText
            color: root.placeholderColor
            font.family: root.font.family
            font.pixelSize: 14
        }
    }

    // ── clear button ──────────────────────────────────────────────────
    UTButton {
        id: clearBtn
        objectName: "clearBtn"
        anchors {
            right: passwordToggle.left
            rightMargin: passwordToggle.visible ? 4 : -20
            verticalCenter: parent.verticalCenter
        }
        visible: root.hasClearButton
        text: "✕"
        buttonType: UTButton.ButtonType.Text
        implicitWidth: 24
        implicitHeight: 24
        font.pixelSize: 14
        padding: 0
        onClicked: {
            root.clear();
            root.forceActiveFocus();
        }
    }

    // ── password visibility toggle ─────────────────────────────────
    Text {
        id: passwordToggle
        objectName: "passwordToggle"
        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter
            rightMargin: 10
        }
        visible: root.passwordMode
        text: root.passwordVisible ? "🙈" : "👁"
        color: root.placeholderColor
        font.pixelSize: 16

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.passwordVisible = !root.passwordVisible
        }
    }
}
