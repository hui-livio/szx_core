/*
 * @file DemoCalendar.qml
 * @brief UTCalendar 组件演示页
 */

// ========================================================================
// ========================================================================

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null

    readonly property var theme: UTTheme.theme

    contentWidth: contentCol.width
    contentHeight: contentCol.implicitHeight

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 24

        // ── 标题 ──────────────────────────────────────────────
        UTLabel {
            text: "Calendar"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        // ── 基础日历 ──────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            UTLabel {
                text: "Basic"
                textRole: UTLabel.TextRole.H4
                color: theme.primaryTextColor
            }

            UTCalendar {
                id: basicCalendar
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                onDateClicked: function (date) {
                    root.toast && root.toast.showToast("Selected: " + date, UTToast.ToastType.Info, 1200);
                }
            }
        }

        // ── 带日程的日历 ──────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            UTLabel {
                text: "With Events"
                textRole: UTLabel.TextRole.H4
                color: theme.primaryTextColor
            }

            UTCalendar {
                id: eventCalendar
                Layout.fillWidth: true
                Layout.preferredHeight: 370

                events: [
                    {
                        date: "2026-06-15",
                        text: "项目评审",
                        color: "#e74c3c"
                    },
                    {
                        date: "2026-06-18",
                        text: "团队周会",
                        color: "#3498db"
                    },
                    {
                        date: "2026-06-18",
                        text: "代码审查",
                        color: "#f39c12"
                    },
                    {
                        date: "2026-06-20",
                        text: "版本发布",
                        color: "#27ae60"
                    },
                    {
                        date: "2026-06-22",
                        text: "客户演示",
                        color: "#9b59b6"
                    },
                    {
                        date: "2026-06-22",
                        text: "技术分享",
                        color: "#9b59b6"
                    },
                    {
                        date: "2026-06-25",
                        text: "月末总结",
                        color: "#e67e22"
                    }
                ]

                onDateClicked: function (date) {
                    var evts = eventCalendar._eventMap[date] || [];
                    var msg = "Selected: " + date;
                    if (evts.length > 0)
                        msg += " (" + evts.length + " events)";
                    root.toast && root.toast.showToast(msg, UTToast.ToastType.Info, 1500);
                }
            }
        }

        // ── 周一为首日 ────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            UTLabel {
                text: "Week Starts Monday"
                textRole: UTLabel.TextRole.H4
                color: theme.primaryTextColor
            }

            UTCalendar {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                firstDayOfWeek: 1
                weekdayNames: ["一", "二", "三", "四", "五", "六", "日"]

                onDateClicked: function (date) {
                    root.toast && root.toast.showToast("Selected: " + date, UTToast.ToastType.Info, 1200);
                }
            }
        }

        // ── 选中日期显示 ─────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            UTLabel {
                text: "Selected Date Info"
                textRole: UTLabel.TextRole.H4
                color: theme.primaryTextColor
            }

            Rectangle {
                Layout.fillWidth: true
                height: 52
                color: theme.surfaceColor
                radius: theme.radius || 8
                border.color: theme.borderColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 16

                    UTLabel {
                        text: "Selected:"
                        textRole: UTLabel.TextRole.Body
                        color: theme.secondaryTextColor
                    }
                    UTLabel {
                        text: basicCalendar.selectedDate
                        textRole: UTLabel.TextRole.Body
                        font.bold: true
                        color: theme.primaryColor
                    }
                    UTLabel {
                        text: "| Viewing: " + basicCalendar.getMonthName(basicCalendar.viewMonth) + " " + basicCalendar.viewYear
                        textRole: UTLabel.TextRole.Body
                        color: theme.secondaryTextColor
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }
}
