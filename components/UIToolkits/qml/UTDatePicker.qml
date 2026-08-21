/*
 * @file UTDatePicker.qml
 * @brief 日期选择器：基于 UTCalendar 实现，支持弹窗选择日期
 */

// ========================================================================
// ========================================================================

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import UIToolkits

Control {
    id: root

    // =====================================================================
    // 主题
    // =====================================================================

    // =====================================================================
    // 公共属性
    // =====================================================================
    property string dateValue: ""
    property int minYear: 1970
    property int maxYear: 2100
    property bool showTodayButton: true
    property string placeholderText: "选择日期"
    property var events: []

    // =====================================================================
    // 信号
    // =====================================================================
    signal valueChanged(string value)

    // =====================================================================
    // 内部状态
    // =====================================================================
    readonly property var _appWindow: Window.window
    property bool _syncing: false
    property date _selectedDate: new Date()
    property string _displayValue: dateValue

    implicitWidth: 280
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 8
    topPadding: 0
    bottomPadding: 0

    // =====================================================================
    // 工具函数
    // =====================================================================
    function pad(n) {
        return (n < 10 ? "0" : "") + n;
    }

    function dateToString(d) {
        return d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate());
    }

    function parseDate(s) {
        const now = new Date();
        const parts = String(s || "").split("-");
        if (parts.length !== 3) {
            return new Date(now.getFullYear(), now.getMonth(), now.getDate());
        }
        const y = Number(parts[0]);
        const m = Number(parts[1]);
        const d = Number(parts[2]);
        if (isNaN(y) || isNaN(m) || isNaN(d)) {
            return new Date(now.getFullYear(), now.getMonth(), now.getDate());
        }
        return new Date(y, m - 1, d);
    }

    function clampDate(d) {
        const minDate = new Date(minYear, 0, 1);
        const maxDate = new Date(maxYear, 11, 31);
        if (d < minDate) {
            return minDate;
        }
        if (d > maxDate) {
            return maxDate;
        }
        return d;
    }

    function syncFromValue() {
        if (_syncing) {
            return;
        }
        _syncing = true;
        _selectedDate = clampDate(parseDate(dateValue));
        _displayValue = dateValue;
        _syncing = false;
    }

    function commitDate(d) {
        const nextDate = clampDate(d);
        _selectedDate = nextDate;
        const str = dateToString(nextDate);
        if (dateValue !== str) {
            _displayValue = str;
            dateValue = str;
            valueChanged(str);
        }
    }

    onDateValueChanged: syncFromValue()

    // =====================================================================
    // 内容项
    // =====================================================================
    contentItem: RowLayout {
        spacing: 8

        UTLabel {
            Layout.fillWidth: true
            text: root._displayValue === "" ? root.placeholderText : root._displayValue
            color: root._displayValue === "" ? Qt.rgba(UTTheme.primaryTextColor.r, UTTheme.primaryTextColor.g, UTTheme.primaryTextColor.b, 0.4) : UTTheme.primaryTextColor
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        UTRoundButton {
            id: calendarBtn
            text: "📅"
            implicitWidth: 32
            implicitHeight: 32
            font.pixelSize: 16
            onClicked: {
                root.syncFromValue();
                pickerPopup.open();
            }
        }
    }

    // =====================================================================
    // 背景
    // =====================================================================
    background: Rectangle {
        radius: UTTheme.radius || 8
        color: UTTheme.backgroundColor
        border.width: 1
        border.color: UTTheme.borderColor
    }

    // =====================================================================
    // 弹窗选择器
    // =====================================================================
    Popup {
        id: pickerPopup
        y: root.height + 6
        width: 340
        padding: 0
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAboutToShow: {
            const globalPos = root.mapToItem(null, 0, 0);
            const winW = root._appWindow ? root._appWindow.width : 9999;
            const globalTop = globalPos.y;
            const globalBottom = root.mapToItem(null, 0, root.height).y;
            const winH = root._appWindow ? root._appWindow.height : 9999;
            const ph = calendar.implicitHeight;

            // 垂直方向：判断向上还是向下弹出
            const targetY = (globalBottom + ph + 10 > winH) ? globalTop - ph - 6 : globalBottom + 6;
            pickerPopup.y = Math.max(-globalTop, Math.min(targetY - globalTop, winH - ph - globalTop));

            // 水平方向：判断左对齐还是右对齐
            const rightEdge = globalPos.x + pickerPopup.width;
            if (rightEdge > winW) {
                // 右侧空间不足，右对齐
                pickerPopup.x = Math.max(-globalPos.x, root.width - pickerPopup.width);
            } else {
                // 左侧空间不足或居中，左对齐
                pickerPopup.x = 0;
            }
        }

        background: Rectangle {
            radius: UTTheme.radius || 8
            color: UTTheme.surfaceColor
            border.width: 1
            border.color: UTTheme.borderColor
        }

        contentItem: UTCalendar {
            id: calendar
            width: implicitWidth
            height: implicitHeight
            selectedDate: root._selectedDate ? root.dateToString(root._selectedDate) : ""
            viewYear: root._selectedDate ? root._selectedDate.getFullYear() : new Date().getFullYear()
            viewMonth: root._selectedDate ? root._selectedDate.getMonth() : new Date().getMonth()
            events: root.events

            onDateClicked: function (date) {
                root.commitDate(root.parseDate(date));
                pickerPopup.close();
            }

            onMonthChanged: function (year, month) {
            // 同步月份变化
            }
        }
    }

    Component.onCompleted: syncFromValue()
}
