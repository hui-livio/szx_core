/*
 * @file DemoCharts.qml
 * @brief UIToolkits 展示应用的 DemoCharts.qml 页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null

    readonly property var theme: UTTheme.theme
    readonly property var areaChartSeries: [
        {
            name: "Mobile",
            color: theme.focusColor,
            data: [
                {
                    month: "Jan",
                    value: 120,
                    label: "January"
                },
                {
                    month: "Feb",
                    value: 180,
                    label: "February"
                },
                {
                    month: "Mar",
                    value: 237,
                    label: "March"
                },
                {
                    month: "Apr",
                    value: 160,
                    label: "April"
                },
                {
                    month: "May",
                    value: 90,
                    label: "May"
                },
                {
                    month: "Jun",
                    value: 200,
                    label: "June"
                }
            ]
        },
        {
            name: "Desktop",
            color: "#f39c12",
            data: [
                {
                    month: "Jan",
                    value: 90,
                    label: "January"
                },
                {
                    month: "Feb",
                    value: 140,
                    label: "February"
                },
                {
                    month: "Mar",
                    value: 170,
                    label: "March"
                },
                {
                    month: "Apr",
                    value: 130,
                    label: "April"
                },
                {
                    month: "May",
                    value: 110,
                    label: "May"
                },
                {
                    month: "Jun",
                    value: 150,
                    label: "June"
                }
            ]
        },
        {
            name: "Tablet",
            color: "#2ecc71",
            data: [
                {
                    month: "Jan",
                    value: 60,
                    label: "January"
                },
                {
                    month: "Feb",
                    value: 80,
                    label: "February"
                },
                {
                    month: "Mar",
                    value: 100,
                    label: "March"
                },
                {
                    month: "Apr",
                    value: 95,
                    label: "April"
                },
                {
                    month: "May",
                    value: 70,
                    label: "May"
                },
                {
                    month: "Jun",
                    value: 120,
                    label: "June"
                }
            ]
        }
    ]

    contentWidth: contentCol.width
    contentHeight: contentCol.implicitHeight

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 24

        UTLabel {
            text: "Charts"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "Pie Chart"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTPieChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "Traffic Sources"
            subtitle: "Website traffic distribution"
            dataSeries: [
                {
                    name: "Sources",
                    data: [
                        {
                            label: "Direct",
                            value: 35,
                            color: "#FF6384"
                        },
                        {
                            label: "Referral",
                            value: 25,
                            color: "#36A2EB"
                        },
                        {
                            label: "Social",
                            value: 20,
                            color: "#FFCE56"
                        },
                        {
                            label: "Organic",
                            value: 15,
                            color: "#4BC0C0"
                        },
                        {
                            label: "Email",
                            value: 5,
                            color: "#9966FF"
                        }
                    ]
                }
            ]
        }

        UTLabel {
            text: "Pie Chart V2 (QSG)"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTPieChartV2 {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "Traffic Sources"
            subtitle: "QSG renderer · website traffic distribution"
            dataSeries: [
                {
                    name: "Sources",
                    data: [
                        {
                            label: "Direct",
                            value: 35,
                            color: "#FF6384"
                        },
                        {
                            label: "Referral",
                            value: 25,
                            color: "#36A2EB"
                        },
                        {
                            label: "Social",
                            value: 20,
                            color: "#FFCE56"
                        },
                        {
                            label: "Organic",
                            value: 15,
                            color: "#4BC0C0"
                        },
                        {
                            label: "Email",
                            value: 5,
                            color: "#9966FF"
                        }
                    ]
                }
            ]
        }

        UTLabel {
            text: "Donut Chart"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTPieChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "Budget Allocation"
            subtitle: "Q4 department budget"
            innerRadius: 40
            dataPoints: [
                {
                    label: "R&D",
                    value: 40,
                    color: "#3a6df0"
                },
                {
                    label: "Marketing",
                    value: 25,
                    color: "#f39c12"
                },
                {
                    label: "Operations",
                    value: 20,
                    color: "#2ecc71"
                },
                {
                    label: "HR",
                    value: 10,
                    color: "#e74c3c"
                },
                {
                    label: "Others",
                    value: 5,
                    color: "#9b59b6"
                }
            ]
        }

        UTLabel {
            text: "Donut Chart V2 (QSG)"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTPieChartV2 {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "Budget Allocation"
            subtitle: "QSG renderer · Q4 department budget"
            innerRadius: 48
            centerText: "100%"
            centerSubText: "Budget"
            dataPoints: [
                {
                    label: "R&D",
                    value: 40,
                    color: "#3a6df0"
                },
                {
                    label: "Marketing",
                    value: 25,
                    color: "#f39c12"
                },
                {
                    label: "Operations",
                    value: 20,
                    color: "#2ecc71"
                },
                {
                    label: "HR",
                    value: 10,
                    color: "#e74c3c"
                },
                {
                    label: "Others",
                    value: 5,
                    color: "#9b59b6"
                }
            ]
        }

        UTLabel {
            text: "Bar Chart"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTBarChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "Monthly Revenue"
            subtitle: "H1 2026 revenue overview"
        }

        UTLabel {
            text: "Bar Chart V2 (QSG)"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTBarChartV2 {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "Inspection Yield"
            subtitle: "QSG renderer · grouped AOI pass rate"
            dataSeries: [
                {
                    name: "Pass",
                    color: "#2ecc71",
                    data: [
                        {
                            label: "Line A",
                            value: 96
                        },
                        {
                            label: "Line B",
                            value: 92
                        },
                        {
                            label: "Line C",
                            value: 89
                        },
                        {
                            label: "Line D",
                            value: 94
                        }
                    ]
                },
                {
                    name: "Review",
                    color: "#f39c12",
                    data: [
                        {
                            label: "Line A",
                            value: 4
                        },
                        {
                            label: "Line B",
                            value: 8
                        },
                        {
                            label: "Line C",
                            value: 11
                        },
                        {
                            label: "Line D",
                            value: 6
                        }
                    ]
                }
            ]
        }

        UTLabel {
            text: "Horizontal Bar Chart"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTHorizontalBarChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            dataPoints: [
                {
                    label: "肖生克的救赎",
                    value: 1000,
                    color: "#3a6df0"
                },
                {
                    label: "教父",
                    value: 9500,
                    color: "#f39c12"
                },
                {
                    label: "钢铁侠",
                    value: 9000,
                    color: "#2ecc71"
                },
                {
                    label: "速度与激情6",
                    value: 895,
                    color: "#e74c3c"
                },
                {
                    label: "速度与激情7",
                    value: 1423,
                    color: "#9b59b6"
                },
                {
                    label: "速度与激情8",
                    value: 6755,
                    color: "#f39c12"
                }
            ]
        }

        UTLabel {
            text: "Value Follow Bar"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTHorizontalBarChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            valueFollowBar: false
            dataPoints: [
                {
                    label: "肖生克的救赎",
                    value: 1000,
                    color: "#3a6df0"
                },
                {
                    label: "教父",
                    value: 9500,
                    color: "#f39c12"
                },
                {
                    label: "钢铁侠",
                    value: 9000,
                    color: "#2ecc71"
                },
                {
                    label: "速度与激情6",
                    value: 895,
                    color: "#e74c3c"
                },
                {
                    label: "速度与激情7",
                    value: 1423,
                    color: "#9b59b6"
                },
                {
                    label: "速度与激情8",
                    value: 6755,
                    color: "#f39c12"
                }
            ]
        }

        UTLabel {
            text: "Area / Line Chart"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTAreaChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "User Growth"
            subtitle: "Monthly active users 2026"
            dataSeries: root.areaChartSeries
        }

        UTLabel {
            text: "Area / Line Chart V2 (QSG)"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        UTAreaChartV2 {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            title: "User Growth"
            subtitle: "Monthly active users 2026"
            dataSeries: root.areaChartSeries
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }
}
