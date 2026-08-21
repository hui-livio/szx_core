/*
 * @file UTBarChartV2.qml
 * @brief UIToolkits 的 UTBarChartV2.qml 组件。
 */

pragma ComponentBehavior: Bound

import QtQuick
import "UTDataUtils.js" as UTDataUtils
import QtQuick.Layouts

Item {
    id: root

    component ThemeText: Text { font.family: UTTheme.fontFamily }

    property string title: ""
    property string subtitle: ""
    property var dataSeries: []
    property var dataPoints: []
    property color barColor: UTTheme.focusColor
    property color backgroundColor: UTTheme.surfaceColor
    property color textColor: UTTheme.primaryTextColor
    property color subtitleColor: UTTheme.secondaryTextColor
    property color borderColor: UTTheme.borderColor
    property color gridColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.10)
    property color tooltipColor: UTTheme.backgroundColor
    property color tooltipTextColor: UTTheme.primaryTextColor
    property real radius: UTTheme.radius
    property real maxValue: 0
    property real barSpacing: 0.2
    property real groupSpacing: 0.3
    property int tooltipTopReserve: 64
    property int xAxisLabelHeight: 28
    property bool titleVisible: true
    property bool xAxisLabelsVisible: true
    property bool seriesVisible: true
    property bool backgroundVisible: true
    property bool gridVisible: true
    readonly property int hoveredSeriesIndex: renderer.hoveredSeriesIndex
    readonly property int hoveredDataIndex: renderer.hoveredDataIndex
    readonly property var hoveredDataPoint: renderer.hoveredDataPoint
    readonly property rect hoveredBarRect: renderer.hoveredBarRect
    readonly property var normalizedDataPoints: UTDataUtils.listValues(dataPoints)
    readonly property var normalizedDataSeries: UTDataUtils.chartSeriesValues(dataSeries)
    readonly property var effectiveDataSeries: normalizedDataPoints.length > 0 ? [
        {
            name: "Data",
            color: root.barColor,
            data: root.normalizedDataPoints
        }
    ] : root.normalizedDataSeries
    readonly property string hoveredSeriesName: hoveredSeriesIndex >= 0 && hoveredSeriesIndex < effectiveDataSeries.length && effectiveDataSeries[hoveredSeriesIndex].name ? String(effectiveDataSeries[hoveredSeriesIndex].name) : ""
    readonly property bool hasTitleText: title.length > 0 || subtitle.length > 0
    readonly property var xAxisLabels: {
        let labels = [];
        if (effectiveDataSeries.length <= 0 || !root.hasOwnPropertyValue(effectiveDataSeries[0], "data"))
            return labels;

        const points = UTDataUtils.listValues(effectiveDataSeries[0].data);
        for (let i = 0; i < points.length; ++i)
            labels.push(root.hasOwnPropertyValue(points[i], "label") ? String(points[i].label) : "");
        return labels;
    }

    signal pointClicked(int seriesIndex, int dataIndex, var dataPoint)
    signal pointHovered(int seriesIndex, int dataIndex, var dataPoint)

    function hasOwnPropertyValue(object, propertyName) {
        return !!object && (typeof object === "object" || typeof object === "function") && propertyName in object;
    }

    function tooltipText(dataPoint) {
        if (!root.hasOwnPropertyValue(dataPoint, "value"))
            return "";

        const labelText = root.hasOwnPropertyValue(dataPoint, "label") ? String(dataPoint.label) : "";
        return labelText + ": " + String(dataPoint.value);
    }

    implicitWidth: 600
    implicitHeight: 500

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        visible: root.backgroundVisible
        color: root.backgroundColor
        border.width: 1
        border.color: root.borderColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Column {
            visible: root.titleVisible && root.hasTitleText
            spacing: 4
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0

            ThemeText {
                width: parent.width
                text: root.title
                color: root.textColor
                font.pixelSize: 18
                font.bold: true
                textFormat: Text.PlainText
                elide: Text.ElideRight
                visible: text.length > 0
            }

            ThemeText {
                width: parent.width
                text: root.subtitle
                color: root.subtitleColor
                font.pixelSize: 12
                textFormat: Text.PlainText
                elide: Text.ElideRight
                visible: text.length > 0
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120

            UTBarChartV2Renderer {
                id: renderer
                dataSeries: root.normalizedDataSeries
                dataPoints: root.normalizedDataPoints
                barColor: root.barColor
                gridColor: root.gridColor
                maxValue: root.maxValue
                barSpacing: root.barSpacing
                groupSpacing: root.groupSpacing
                gridVisible: root.gridVisible

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                    topMargin: root.tooltipTopReserve
                    bottomMargin: root.xAxisLabelsVisible ? root.xAxisLabelHeight : 0
                }

                onPointClicked: function (seriesIndex, dataIndex, dataPoint) {
                    root.pointClicked(seriesIndex, dataIndex, dataPoint);
                }

                onPointHovered: function (seriesIndex, dataIndex, dataPoint) {
                    root.pointHovered(seriesIndex, dataIndex, dataPoint);
                }
            }

            Rectangle {
                id: tooltip
                readonly property real targetX: {
                    let barCenterX = renderer.x + renderer.hoveredBarRect.x + renderer.hoveredBarRect.width / 2;
                    if (parent.width > width)
                        return Math.max(0, Math.min(barCenterX - width / 2, parent.width - width));
                    return Math.max(0, renderer.x + renderer.hoveredBarRect.x - width - 8);
                }
                readonly property real targetY: {
                    let aboveY = renderer.y + renderer.hoveredBarRect.y - height - 8;
                    return Math.max(0, aboveY);
                }

                visible: renderer.hoveredDataIndex >= 0
                opacity: renderer.hoveredDataIndex >= 0 ? 1 : 0
                x: targetX
                y: targetY
                width: Math.max(tooltipColumn.implicitWidth + 20, 96)
                height: tooltipColumn.implicitHeight + 16
                radius: 8
                color: root.tooltipColor
                border.width: 1
                border.color: UTTheme.focusColor
                z: 2

                Behavior on x {
                    enabled: renderer.hoveredDataIndex >= 0
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on y {
                    enabled: renderer.hoveredDataIndex >= 0
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 200
                    }
                }

                Column {
                    id: tooltipColumn
                    x: 10
                    y: 8
                    spacing: 4

                    ThemeText {
                        text: root.hoveredSeriesName
                        color: root.tooltipTextColor
                        font.pixelSize: 13
                        font.bold: true
                        textFormat: Text.PlainText
                        visible: text.length > 0
                    }

                    Row {
                        spacing: 5

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: root.hoveredSeriesIndex >= 0 && root.hoveredSeriesIndex < root.effectiveDataSeries.length ? root.effectiveDataSeries[root.hoveredSeriesIndex].color || root.barColor : root.barColor
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        ThemeText {
                            text: root.tooltipText(renderer.hoveredDataPoint)
                            color: root.tooltipTextColor
                            font.pixelSize: 13
                            textFormat: Text.PlainText
                            anchors.verticalCenter: parent.verticalCenter
                        }
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
                }

                Repeater {
                    model: root.xAxisLabels

                    ThemeText {
                        required property string modelData

                        width: xAxisLabelRow.width / Math.max(1, root.xAxisLabels.length)
                        height: xAxisLabelRow.height
                        text: modelData
                        color: root.subtitleColor
                        font.pixelSize: 11
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Row {
            spacing: 16
            visible: root.seriesVisible && root.effectiveDataSeries.length > 1
            Layout.fillWidth: true

            Repeater {
                model: root.effectiveDataSeries

                Row {
                    id: legendItem
                    required property var modelData
                    spacing: 6

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 2
                        color: legendItem.modelData.color || root.barColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    ThemeText {
                        text: legendItem.modelData.name || ""
                        color: root.subtitleColor
                        font.pixelSize: 12
                        textFormat: Text.PlainText
                    }
                }
            }
        }
    }
}
