/*
 * @file UTToast.qml
 * @brief UIToolkits 的 UTToast.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

Popup {
    id: root

    enum ToastType {
        Info,
        Success,
        Warning,
        Error
    }
    property int toastType: UTToast.ToastType.Info
    property int duration: 2200
    property string message: ""

    // 无障碍（Popup 不是 Item，挂到 contentItem）
    property string accName: ""
    property int accRole: Accessible.AlertMessage

    x: (parent ? (parent.width - width) / 2 : 0)
    y: parent ? parent.height - height - 36 : 0
    modal: false
    focus: false
    closePolicy: Popup.CloseOnEscape
    padding: 14

    background: Rectangle {
        radius: Math.round((UTTheme.radius || 8) * 1.25)
        color: {
            switch (root.toastType) {
            case UTToast.ToastType.Success:
                return "#2e9e5b";
            case UTToast.ToastType.Warning:
                return "#e69a2a";
            case UTToast.ToastType.Error:
                return "#d94848";
            default:
                return UTTheme.primaryColor;
            }
        }
        opacity: 0.95
    }

    contentItem: Label {
        text: root.message
        color: "#ffffff"
        font.pixelSize: 14
        font.family: UTTheme.fontFamily
        wrapMode: Text.WordWrap

        Accessible.name: root.accName !== "" ? root.accName : root.message
        Accessible.role: root.accRole
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 160
            }
            NumberAnimation {
                property: "scale"
                from: 0.96
                to: 1.0
                duration: 160
            }
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 160
        }
    }

    Timer {
        id: closeTimer
        interval: root.duration
        repeat: false
        onTriggered: root.close()
    }

    function showToast(text, type, ms) {
        root.message = text;
        if (type !== undefined) {
            root.toastType = type;
        }
        if (ms !== undefined) {
            root.duration = ms;
        }
        closeTimer.restart();
        root.open();
    }
}
