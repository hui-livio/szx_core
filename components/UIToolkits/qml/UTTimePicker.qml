/*
 * @file UTTimePicker.qml
 * @brief UIToolkits 的 UTTimePicker.qml 组件。
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

Control {
    id: root

    property string timeValue: ""
    property bool showSeconds: false

    signal valueChanged(string value)

    readonly property var _appWindow: Window.window

    property bool _syncing: false
    property string _displayValue: timeValue

    implicitWidth: 250
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 8
    topPadding: 0
    bottomPadding: 0

    function pad(n) {
        return (n < 10 ? "0" : "") + n;
    }

    function composeTime(h, m, s) {
        const base = pad(h) + ":" + pad(m);
        return showSeconds ? (base + ":" + pad(s)) : base;
    }

    function parseTime(text) {
        const now = new Date();
        const parts = String(text || "").split(":");
        if (parts.length < 2) {
            return [now.getHours(), now.getMinutes(), now.getSeconds()];
        }
        const h = Number(parts[0]);
        const m = Number(parts[1]);
        const s = parts.length > 2 ? Number(parts[2]) : now.getSeconds();
        return [isNaN(h) ? now.getHours() : Math.max(0, Math.min(23, h)), isNaN(m) ? now.getMinutes() : Math.max(0, Math.min(59, m)), isNaN(s) ? now.getSeconds() : Math.max(0, Math.min(59, s))];
    }

    function syncFromValue() {
        if (_syncing) {
            return;
        }
        _syncing = true;
        const parts = parseTime(timeValue);
        hourTumbler.currentIndex = parts[0];
        minuteTumbler.currentIndex = parts[1];
        secondTumbler.currentIndex = parts[2];
        _displayValue = timeValue;
        _syncing = false;
    }

    function commit(h, m, s) {
        const next = composeTime(h, m, s);
        if (_displayValue !== next) {
            _displayValue = next;
            timeValue = next;
            valueChanged(next);
        }
    }

    function openPicker() {
        syncFromValue();
        pickerPopup.open();
    }

    onTimeValueChanged: syncFromValue()

    contentItem: RowLayout {
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: root._displayValue === "" ? qsTr("Select time") : root._displayValue
            color: UTTheme.primaryTextColor
            font.family: UTTheme.fontFamily
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Button {
            text: "Pick"
            onClicked: root.openPicker()
        }
    }

    background: Rectangle {
        radius: UTTheme.radius || 8
        color: UTTheme.backgroundColor
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.14)
    }

    Popup {
        id: pickerPopup
        y: root.height + 6
        width: root.showSeconds ? 320 : 250
        padding: 12
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAboutToShow: {
            const globalX = root.mapToItem(null, 0, 0).x;
            const winW = root._appWindow ? root._appWindow.width : 9999;
            const globalTop = root.mapToItem(null, 0, 0).y;
            const globalBottom = root.mapToItem(null, 0, root.height).y;
            const winH = root._appWindow ? root._appWindow.height : 9999;
            const ph = pickerPopup.implicitHeight > 0 ? pickerPopup.implicitHeight : 220;
            const targetY = (globalBottom + ph + 10 > winH) ? globalTop - ph - 6 : globalBottom + 6;
            pickerPopup.y = Math.max(-globalTop, Math.min(targetY - globalTop, winH - ph - globalTop));
            pickerPopup.x = Math.max(-globalX, Math.min(0, winW - globalX - pickerPopup.width));
        }

        background: Rectangle {
            radius: Math.round((UTTheme.radius || 8) * 1.25)
            color: UTTheme.backgroundColor
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.16)
        }

        contentItem: ColumnLayout {
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Tumbler {
                    id: hourTumbler
                    Layout.fillWidth: true
                    model: 24
                    delegate: Label {
                        required property int modelData
                        text: root.pad(modelData)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        opacity: 0.9
                        font.family: UTTheme.fontFamily
                        color: UTTheme.primaryTextColor
                    }
                }

                Label {
                    text: ":"
                    font.pixelSize: 18
                    font.family: UTTheme.fontFamily
                    color: UTTheme.secondaryTextColor
                }

                Tumbler {
                    id: minuteTumbler
                    Layout.fillWidth: true
                    model: 60
                    delegate: Label {
                        required property int modelData
                        text: root.pad(modelData)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        opacity: 0.9
                        font.family: UTTheme.fontFamily
                        color: UTTheme.primaryTextColor
                    }
                }

                Label {
                    visible: root.showSeconds
                    text: ":"
                    font.pixelSize: 18
                    font.family: UTTheme.fontFamily
                    color: UTTheme.secondaryTextColor
                }

                Tumbler {
                    id: secondTumbler
                    visible: root.showSeconds
                    Layout.fillWidth: true
                    model: 60
                    delegate: Label {
                        required property int modelData
                        text: root.pad(modelData)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        opacity: 0.9
                        font.family: UTTheme.fontFamily
                        color: UTTheme.primaryTextColor
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: "Now"
                    onClicked: {
                        const now = new Date();
                        hourTumbler.currentIndex = now.getHours();
                        minuteTumbler.currentIndex = now.getMinutes();
                        secondTumbler.currentIndex = now.getSeconds();
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: "Cancel"
                    onClicked: pickerPopup.close()
                }

                Button {
                    text: "OK"
                    onClicked: {
                        root.commit(hourTumbler.currentIndex, minuteTumbler.currentIndex, secondTumbler.currentIndex);
                        pickerPopup.close();
                    }
                }
            }
        }
    }

    Component.onCompleted: syncFromValue()
}
