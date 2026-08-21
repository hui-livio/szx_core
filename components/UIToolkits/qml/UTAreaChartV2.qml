/*
 * @file UTAreaChartV2.qml
 * @brief UIToolkits 的 UTAreaChartV2.qml 组件。
 */

pragma ComponentBehavior: Bound

import QtQuick
import "UTDataUtils.js" as UTDataUtils

Item {
    id: root

    component ThemeText: Text { font.family: UTTheme.fontFamily }

    enum LineStyle {
        Smooth,
        Linear,
        Step
    }

    property string title: "Area Chart"
    property string subtitle: "以下是前六个月用户访问数"
    property var dataSeries: [
        {
            name: "Mobile",
            color: UTTheme.focusColor,
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
        }
    ]
    property var dataPoints: []
    property int lineStyle: UTAreaChartV2.LineStyle.Smooth
    property color backgroundColor: UTTheme.surfaceColor
    property color textColor: UTTheme.primaryTextColor
    property color subtitleColor: UTTheme.primaryTextColor
    property color borderColor: UTTheme.borderColor
    property color tooltipColor: UTTheme.backgroundColor
    property color tooltipTextColor: UTTheme.primaryTextColor
    property real radius: 20
    property int fontSize: 14
    property int titleFontSize: 18
    property int subtitleFontSize: 12
    property int chartPadding: 20
    property int topPadding: 90
    property int tooltipTopReserve: 56
    property int xAxisLabelHeight: 28
    property int antialiasingSamples: 4
    property bool titleVisible: true
    property bool xAxisLabelsVisible: true
    property bool legendVisible: true
    property bool styleSwitchVisible: true
    property bool backgroundVisible: true

    readonly property var normalizedDataPoints: UTDataUtils.listValues(dataPoints)
    readonly property var normalizedDataSeries: UTDataUtils.chartSeriesValues(dataSeries)
    readonly property var effectiveDataSeries: normalizedDataPoints.length > 0 ? [
        {
            name: "Data",
            color: UTTheme.focusColor,
            data: root.normalizedDataPoints
        }
    ] : root.normalizedDataSeries
    readonly property var xAxisLabels: {
        let labels = [];
        if (effectiveDataSeries.length <= 0 || !root.hasOwnPropertyValue(effectiveDataSeries[0], "data"))
            return labels;

        const points = UTDataUtils.listValues(effectiveDataSeries[0].data);
        for (let i = 0; i < points.length; ++i) {
            const point = points[i];
            if (root.hasOwnPropertyValue(point, "month"))
                labels.push(String(point.month));
            else if (root.hasOwnPropertyValue(point, "label"))
                labels.push(String(point.label));
            else
                labels.push("");
        }
        return labels;
    }
    readonly property bool hasTitleText: title.length > 0 || subtitle.length > 0
    readonly property int hoveredIndex: renderer.hoveredIndex
    readonly property var hoveredDataPoint: renderer.hoveredDataPoint
    readonly property point hoveredPoint: renderer.hoveredPoint

    signal pointClicked(int index, var dataPoint)
    signal pointHovered(int index, var dataPoint)

    function hasOwnPropertyValue(object, propertyName) {
        return !!object && (typeof object === "object" || typeof object === "function") && propertyName in object;
    }

    function tooltipTitle(dataPoint) {
        if (!dataPoint)
            return "";
        if (root.hasOwnPropertyValue(dataPoint, "label"))
            return String(dataPoint.label);
        if (root.hasOwnPropertyValue(dataPoint, "month"))
            return String(dataPoint.month);
        return "";
    }

    function switchLineStyle() {
        if (root.lineStyle === UTAreaChartV2.LineStyle.Smooth)
            root.lineStyle = UTAreaChartV2.LineStyle.Linear;
        else if (root.lineStyle === UTAreaChartV2.LineStyle.Linear)
            root.lineStyle = UTAreaChartV2.LineStyle.Step;
        else
            root.lineStyle = UTAreaChartV2.LineStyle.Smooth;
    }

    function lineStyleText() {
        if (root.lineStyle === UTAreaChartV2.LineStyle.Linear)
            return "Linear";
        if (root.lineStyle === UTAreaChartV2.LineStyle.Step)
            return "Step";
        return "Smooth";
    }

    implicitWidth: 600
    implicitHeight: 500

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        visible: root.backgroundVisible
        color: root.backgroundColor
        border.width: 0
        border.color: root.borderColor
    }

    Column {
        id: titleColumn
        spacing: 5
        visible: root.titleVisible && root.hasTitleText
        width: parent.width - (styleButton.visible ? styleButton.width + 96 : 80)

        anchors {
            top: parent.top
            left: parent.left
            margins: 40
        }

        ThemeText {
            width: parent.width
            text: root.title
            color: root.textColor
            font.pixelSize: root.titleFontSize
            font.bold: true
            textFormat: Text.PlainText
            elide: Text.ElideRight
            visible: text.length > 0
        }

        ThemeText {
            width: parent.width
            text: root.subtitle
            color: root.subtitleColor
            font.pixelSize: root.subtitleFontSize
            textFormat: Text.PlainText
            elide: Text.ElideRight
            visible: text.length > 0
        }
    }

    Rectangle {
        id: styleButton
        visible: root.styleSwitchVisible
        width: 80
        height: 32
        radius: 16
        color: root.backgroundColor
        border.width: root.backgroundVisible ? 1 : 0
        border.color: root.borderColor

        anchors {
            top: parent.top
            right: parent.right
            margins: 40
        }

        ThemeText {
            anchors.centerIn: parent
            text: root.lineStyleText()
            color: root.textColor
            font.pixelSize: 12
            textFormat: Text.PlainText
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: UTTheme.focusColor
            opacity: styleButtonMouseArea.containsMouse ? 0.1 : 0

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }
        }

        MouseArea {
            id: styleButtonMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.switchLineStyle()
        }
    }

    Item {
        id: chartArea

        anchors {
            fill: parent
            topMargin: root.topPadding
            margins: root.chartPadding
        }

        UTAreaChartV2Renderer {
            id: renderer
            dataSeries: root.normalizedDataSeries
            dataPoints: root.normalizedDataPoints
            lineStyle: root.lineStyle

            layer {
                enabled: root.antialiasingSamples > 1
                samples: root.antialiasingSamples
                smooth: true
            }

            anchors {
                fill: parent
                margins: 8
                bottomMargin: legend.visible ? 100 : 40
            }

            onPointClicked: function (index, dataPoint) {
                root.pointClicked(index, dataPoint);
            }

            onPointHovered: function (index, dataPoint) {
                root.pointHovered(index, dataPoint);
            }
        }
    }

    Row {
        id: xAxisLabelRow
        height: root.xAxisLabelHeight
        visible: root.xAxisLabelsVisible && root.xAxisLabels.length > 0

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: root.chartPadding
            bottomMargin: legend.visible ? 75 : 15
        }

        Repeater {
            model: root.xAxisLabels

            ThemeText {
                required property string modelData
                width: xAxisLabelRow.width / Math.max(1, root.xAxisLabels.length)
                height: xAxisLabelRow.height
                text: modelData
                color: root.subtitleColor
                font.pixelSize: root.fontSize - 2
                textFormat: Text.PlainText
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Rectangle {
        id: tooltip
        visible: renderer.hoveredIndex >= 0
        opacity: renderer.hoveredIndex >= 0 ? 1 : 0
        x: Math.max(10, Math.min(chartArea.x + renderer.x + renderer.hoveredPoint.x - width / 2, root.width - width - 10))
        y: Math.max(10, chartArea.y + renderer.y + renderer.hoveredPoint.y - height - 10)
        width: tooltipColumn.implicitWidth + 20
        height: tooltipColumn.implicitHeight + 16
        radius: 8
        color: root.tooltipColor
        border.width: 1
        border.color: UTTheme.focusColor
        z: 2

        Behavior on x {
            enabled: renderer.hoveredIndex >= 0
            NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
        }

        Behavior on y {
            enabled: renderer.hoveredIndex >= 0
            NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }

        Column {
            id: tooltipColumn
            x: 10
            y: 8
            spacing: 4

            ThemeText {
                text: root.tooltipTitle(renderer.hoveredDataPoint)
                color: root.tooltipTextColor
                font.pixelSize: root.fontSize - 1
                font.bold: true
                textFormat: Text.PlainText
                visible: text.length > 0
            }

            Repeater {
                model: root.effectiveDataSeries

                Row {
                    id: tooltipSeriesItem
                    required property var modelData
                    spacing: 5

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: tooltipSeriesItem.modelData.color || UTTheme.focusColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    ThemeText {
                        text: tooltipSeriesItem.modelData.name || ""
                        color: root.subtitleColor
                        font.pixelSize: root.fontSize - 2
                        textFormat: Text.PlainText
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    ThemeText {
                        text: renderer.hoveredIndex >= 0 && root.hasOwnPropertyValue(tooltipSeriesItem.modelData, "data") && tooltipSeriesItem.modelData.data.length > renderer.hoveredIndex ? String(tooltipSeriesItem.modelData.data[renderer.hoveredIndex].value) : ""
                        color: root.tooltipTextColor
                        font.pixelSize: root.fontSize - 1
                        font.bold: true
                        textFormat: Text.PlainText
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    Row {
        id: legend
        spacing: 30
        visible: root.legendVisible && (root.effectiveDataSeries.length > 1 || (root.effectiveDataSeries.length === 1 && !(root.effectiveDataSeries[0].name === "Data")))

        anchors {
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
            bottomMargin: 20
        }

        Repeater {
            model: root.effectiveDataSeries

            Row {
                id: legendItem
                required property var modelData
                spacing: 8

                Rectangle {
                    width: 12
                    height: 12
                    radius: 2
                    color: legendItem.modelData.color || UTTheme.focusColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                ThemeText {
                    text: legendItem.modelData.name || ""
                    color: root.subtitleColor
                    font.pixelSize: root.fontSize - 2
                    textFormat: Text.PlainText
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
