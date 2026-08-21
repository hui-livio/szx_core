/*
 * @file UTCombobox.qml
 * @brief UIToolkits 的 UTCombobox.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: root

    // ── Theme ────────────────────────────────────────────────────

    // ── Accessibility ────────────────────────────────────────────
    property string placeholderText: ""
    property string accName: root.displayText || root.placeholderText || ""
    property int accRole: Accessible.ComboBox
    Accessible.name: accName
    Accessible.role: accRole

    // ── Theme-aware colors ───────────────────────────────────────
    property color primaryColor: UTTheme.primaryColor
    property color textColor: UTTheme.primaryTextColor
    property color placeholderColor: UTTheme.secondaryTextColor
    property color fieldBg: UTTheme.surfaceColor
    property color fieldBgDisabled: {
        var c = fieldBg;
        return Qt.rgba(c.r * 0.92, c.g * 0.92, c.b * 0.92, c.a);
    }
    property color borderNormal: UTTheme.borderColor
    property color borderHover: primaryColor
    property color borderFocus: primaryColor
    property color borderDisabled: {
        var c = borderNormal;
        return Qt.rgba(c.r, c.g, c.b, c.a * 0.4);
    }
    property color arrowColor: placeholderColor
    property color arrowHoverColor: primaryColor
    property color popupBg: fieldBg
    property color popupBorder: borderNormal
    property color delegateSelectedBg: {
        var c = primaryColor;
        return Qt.rgba(c.r, c.g, c.b, 0.12);
    }
    property color delegateHoverBg: {
        var c = primaryColor;
        return Qt.rgba(c.r, c.g, c.b, 0.06);
    }
    property string indicatorText: ""
    property color indicatorTextColor: placeholderColor

    // ── Sizing ───────────────────────────────────────────────────
    implicitWidth: 200
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 36
    topPadding: 8
    bottomPadding: 8
    font.pixelSize: 14
    flat: false

    // ── contentItem ──────────────────────────────────────────────
    contentItem: TextField {
        topPadding: 0
        bottomPadding: 0
        text: root.editable ? root.editText : root.displayText
        color: {
            if (!root.enabled)
                return root.borderDisabled;
            if (!root.displayText && !root.editText)
                return root.placeholderColor;
            return root.textColor;
        }
        font: root.font
        enabled: root.editable
        autoScroll: root.editable
        readOnly: root.down
        selectByMouse: root.selectTextByMouse
        verticalAlignment: Text.AlignVCenter
        clip: true

        background: Item {}

        placeholderText: root.placeholderText || ""
        placeholderTextColor: root.placeholderColor
    }

    // ── background ───────────────────────────────────────────────
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
        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
    }

    // ── indicator ────────────────────────────────────────────────
    indicator: Text {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 6
        text: root.indicatorText || ""
        font.pixelSize: 14
        color: root.enabled ? (hoverHandler.hovered ? root.arrowHoverColor : root.arrowColor) : root.borderDisabled
        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
    }

    // ── popup ────────────────────────────────────────────────────
    popup: Popup {
        y: root.height - 1
        implicitWidth: Math.max(root.width, 180)
        implicitHeight: Math.min(listView.contentHeight + padding * 2, 280)
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Math.round((UTTheme.radius || 8) * 0.667)
            color: root.popupBg
            border.width: 1
            border.color: root.popupBorder
        }

        contentItem: ListView {
            id: listView
            clip: true
            model: root.delegateModel
            currentIndex: root.highlightedIndex
            implicitHeight: contentHeight

            highlight: Rectangle {
                radius: 4
                color: root.delegateSelectedBg
                Behavior on y {
                    NumberAnimation {
                        duration: 120
                        easing.type: Easing.OutCubic
                    }
                }
            }
            highlightMoveDuration: 120
            highlightResizeDuration: 0

            delegate: ItemDelegate {
                id: delegateRoot
                required property var model
                required property var modelData
                required property int index
                width: ListView.view.width
                implicitHeight: 36
                text: {
                    var d = delegateRoot.modelData;
                    if (d === null || d === undefined)
                        return "";
                    // 字符串/数字数组: modelData 即为文本值本身
                    if (typeof d === "string" || typeof d === "number")
                        return String(d);
                    // QStringListModel: 使用 display 属性
                    if (d.display !== undefined)
                        return String(d.display);
                    // ListModel 或对象数组: 通过 textRole 取值
                    var role = root.textRole || "text";
                    if (d[role] !== undefined)
                        return String(d[role]);
                    return String(d);
                }
                highlighted: root.highlightedIndex === delegateRoot.index
                hoverEnabled: true

                contentItem: Text {
                    text: delegateRoot.text
                    color: root.enabled ? root.textColor : root.borderDisabled
                    font: root.font
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                    rightPadding: 12
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: 4
                    color: {
                        if (delegateRoot.highlighted)
                            return root.delegateSelectedBg;
                        if (delegateRoot.hovered)
                            return root.delegateHoverBg;
                        return "transparent";
                    }
                    Behavior on color {
                        ColorAnimation {
                            duration: 100
                        }
                    }
                }

                onClicked: {
                    root.currentIndex = delegateRoot.index;
                    root.activated(delegateRoot.index);
                    root.popup.close();
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }

    // ── Hover cursor ────────────────────────────────────────────
    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
