/*
 * @file UTCalendar.qml
 * @brief 日历组件：圆形日期按钮、事件圆点标记、hover tooltip
 */

// ========================================================================
// ========================================================================

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

Item {
    id: root

    // =====================================================================
    // 主题
    // =====================================================================

    // =====================================================================
    // 公共属性
    // =====================================================================
    property string selectedDate: dateToString(new Date())
    property int viewYear: new Date().getFullYear()
    property int viewMonth: new Date().getMonth()
    property int firstDayOfWeek: 0
    /// 外部可覆盖; 为空时自动用 Qt.locale()
    property var weekdayNames: []
    property var monthNames: []
    /// 日程: [{date:"YYYY-MM-DD", text:"...", color:"#xxx"}, ...]
    property var events: []

    // =====================================================================
    // 信号
    // =====================================================================
    signal dateClicked(string date)
    signal monthChanged(int year, int month)

    implicitWidth: 320
    implicitHeight: 320

    // =====================================================================
    // 工具函数
    // =====================================================================
    function pad(n) {
        return (n < 10 ? "0" : "") + n;
    }
    function dateToString(d) {
        return d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate());
    }

    // 获取星期名: 外部设置按位置索引，否则 Qt.locale() 兜底
    function getWeekdayName(pos) {
        if (weekdayNames.length > 0)
            return weekdayNames[pos % 7];
        // Qt.locale(): 0=Sunday, 1=Monday...
        var day = (pos + firstDayOfWeek) % 7;
        return Qt.locale().standaloneDayName(day, Locale.ShortFormat);
    }
    // 获取月份名: 外部设置优先，否则 Qt.locale() 兜底
    function getMonthName(month) {
        if (monthNames.length > 0)
            return monthNames[month];
        return Qt.locale().standaloneMonthName(month, Locale.LongFormat);
    }

    readonly property int _daysInMonth: new Date(viewYear, viewMonth + 1, 0).getDate()
    readonly property int _startDayOffset: {
        var d = new Date(viewYear, viewMonth, 1).getDay();
        return (d - firstDayOfWeek + 7) % 7;
    }
    /// 动态计算需要的格子数（5行或6行）
    readonly property int _totalCells: {
        var cells = _startDayOffset + _daysInMonth;
        return Math.ceil(cells / 7) * 7;
    }
    property date _today: new Date()
    readonly property string _todayStr: dateToString(_today)

    /// 按日期分组的日程映射: { "YYYY-MM-DD": [{text, color}, ...] }
    readonly property var _eventMap: {
        var m = {};
        for (var i = 0; i < events.length; i++) {
            var e = events[i];
            if (!e.date)
                continue;
            if (!m[e.date])
                m[e.date] = [];
            m[e.date].push({
                text: e.text || "",
                color: e.color || ""
            });
        }
        return m;
    }

    Timer {
        interval: 60000
        repeat: true
        running: root.visible
        onTriggered: root._today = new Date()
    }

    function goToPrevMonth() {
        if (viewMonth === 0) {
            viewMonth = 11;
            viewYear--;
        } else {
            viewMonth--;
        }
        monthChanged(viewYear, viewMonth);
    }

    function goToNextMonth() {
        if (viewMonth === 11) {
            viewMonth = 0;
            viewYear++;
        } else {
            viewMonth++;
        }
        monthChanged(viewYear, viewMonth);
    }

    function selectDate(day) {
        selectedDate = dateToString(new Date(viewYear, viewMonth, day));
        dateClicked(selectedDate);
    }

    // =====================================================================
    // Tooltip 显示状态
    // =====================================================================
    property string _tooltipDate: ""
    property point _tooltipPos: Qt.point(0, 0)
    readonly property var _tooltipEvents: _tooltipDate ? (_eventMap[_tooltipDate] || []) : []

    // =====================================================================
    // 背景
    // =====================================================================
    Rectangle {
        anchors.fill: parent
        color: UTTheme.surfaceColor
        radius: UTTheme.radius || 8
        border.color: UTTheme.borderColor
        border.width: 1
    }

    // =====================================================================
    // 主布局
    // =====================================================================
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        // ── 月份导航 ────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            UTRoundButton {
                text: "<"
                implicitWidth: 32
                implicitHeight: 32
                font.pixelSize: 14
                onClicked: root.goToPrevMonth()
            }
            UTLabel {
                text: root.getMonthName(root.viewMonth) + " " + root.viewYear
                textRole: UTLabel.TextRole.Body
                font.bold: true
                color: UTTheme.primaryTextColor
                font.pixelSize: 15
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            UTRoundButton {
                text: ">"
                implicitWidth: 32
                implicitHeight: 32
                font.pixelSize: 14
                onClicked: root.goToNextMonth()
            }
        }

        // ── 星期标题 ────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            Repeater {
                model: 7
                Text {
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    text: root.getWeekdayName(index)
                    font.pixelSize: 11
                    font.family: UTTheme.fontFamily
                    color: Qt.rgba(UTTheme.primaryTextColor.r, UTTheme.primaryTextColor.g, UTTheme.primaryTextColor.b, 0.5)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // ── 日期网格 ────────────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root._totalCells / 7 * 40
            columns: 7
            columnSpacing: 2
            rowSpacing: 2

            Repeater {
                model: root._totalCells

                Item {
                    required property int index
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    property int day: index - root._startDayOffset + 1
                    readonly property bool _in: day > 0 && day <= root._daysInMonth
                    readonly property string _ds: _in ? root.viewYear + "-" + root.pad(root.viewMonth + 1) + "-" + root.pad(day) : ""
                    readonly property bool _today: _in && _ds === root._todayStr
                    readonly property bool _sel: _in && _ds === root.selectedDate
                    readonly property var _evts: _in ? (root._eventMap[_ds] || []) : []
                    readonly property int _evtCount: _evts.length
                    // 单事件取其颜色；多事件或无数事件用主题色
                    readonly property color _dotColor: _evtCount === 1 && _evts[0].color ? _evts[0].color : (UTTheme.primaryColor)

                    // ── 圆形日期按钮 ──────────────────────────
                    Rectangle {
                        id: dateCircle
                        anchors.centerIn: parent
                        width: Math.min(parent.width, parent.height) * 0.78
                        height: width
                        radius: width / 2

                        color: {
                            if (!_in)
                                return "transparent";
                            if (_today)
                                return UTTheme.primaryColor;
                            if (cellMa.containsMouse)
                                return Qt.rgba(UTTheme.primaryColor.r, UTTheme.primaryColor.g, UTTheme.primaryColor.b, 0.08);
                            return "transparent";
                        }
                        border.width: _sel && !_today ? 2 : 0
                        border.color: UTTheme.primaryColor

                        Text {
                            anchors.centerIn: parent
                            text: _in ? day : ""
                            font.pixelSize: 13
                            font.bold: _today || _sel
                            font.family: UTTheme.fontFamily
                            color: _today ? "#ffffff" : (_in ? UTTheme.primaryTextColor : Qt.rgba(UTTheme.primaryTextColor.r, UTTheme.primaryTextColor.g, UTTheme.primaryTextColor.b, 0.25))
                        }
                    }

                    // ── 事件圆点 ──────────────────────────────
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 5
                        height: 5
                        radius: 2.5
                        color: _dotColor
                        visible: _in && _evtCount > 0
                    }

                    // ── 交互 ──────────────────────────────────
                    MouseArea {
                        id: cellMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: _in ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (_in)
                                root.selectDate(day);
                        }
                        onEntered: {
                            if (_in && _evtCount > 0) {
                                root._tooltipDate = _ds;
                                var pt = mapToItem(root, mouseX, mouseY);
                                root._tooltipPos = Qt.point(pt.x, pt.y);
                            }
                        }
                        onExited: {
                            root._tooltipDate = "";
                        }
                        onPositionChanged: {
                            if (_in && _evtCount > 0) {
                                var pt = mapToItem(root, mouseX, mouseY);
                                root._tooltipPos = Qt.point(pt.x, pt.y);
                            }
                        }
                    }
                }
            }
        }
    }

    // =====================================================================
    // 事件 Tooltip
    // =====================================================================
    Rectangle {
        id: tooltip
        visible: root._tooltipEvents.length > 0
        x: Math.max(4, Math.min(root._tooltipPos.x + 12, root.width - tooltip.width - 4))
        y: Math.max(4, root._tooltipPos.y - tooltip.height - 8)
        width: Math.min(root.width - 8, Math.max(tooltipCol.implicitWidth + 16, 120))
        height: tooltipCol.implicitHeight + 12
        radius: 6
        color: Qt.rgba(0, 0, 0, 0.82)
        z: 100

        Column {
            id: tooltipCol
            anchors.centerIn: parent
            spacing: 3

            Repeater {
                model: root._tooltipEvents
                Row {
                    required property var modelData
                    spacing: 5
                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: modelData.color || (UTTheme.primaryColor)
                    }
                    Text {
                        text: modelData.text
                        font.family: UTTheme.fontFamily
                        width: Math.max(80, tooltip.width - 32)
                        font.pixelSize: 11
                        color: "#ffffff"
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
