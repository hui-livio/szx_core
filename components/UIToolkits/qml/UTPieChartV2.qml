/*
 * @file UTPieChartV2.qml
 * @brief UIToolkits 的 UTPieChartV2.qml 组件。
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: root

    component ThemeText: Text { font.family: UTTheme.fontFamily }

    property string title: "Pie Chart"
    property string subtitle: "数据占比分析"
    property var dataSeries: []
    property var dataPoints: []
    property color backgroundColor: UTTheme.surfaceColor
    property color textColor: UTTheme.primaryTextColor
    property color subtitleColor: UTTheme.secondaryTextColor
    property color borderColor: UTTheme.borderColor
    property color tooltipColor: UTTheme.backgroundColor
    property color tooltipTextColor: UTTheme.primaryTextColor
    property color shadowColor: UTTheme.shadowColor
    property real radius: UTTheme.radius
    property real innerRadius: 0
    property string centerText: ""
    property string centerSubText: ""
    property int percentPrecision: 2
    property int tooltipReserve: 42
    property int antialiasingSamples: 4
    property real contentMargins: 24
    property bool shadowEnabled: true
    property bool titleVisible: true
    property bool labelsVisible: true
    property bool legendVisible: true
    property bool percentInLegend: true
    property bool backgroundVisible: true
    readonly property color effectiveBorderColor: Qt.rgba(borderColor.r || 0.84, borderColor.g || 0.86, borderColor.b || 0.89, 0.4)

    readonly property var normalizedDataPoints: root.isListLike(dataPoints) ? dataPoints : []
    readonly property var normalizedDataSeries: root.isListLike(dataSeries) ? dataSeries : []
    readonly property var effectiveData: {
        let source = [];
        if (root.normalizedDataPoints.length > 0) {
            source = root.normalizedDataPoints;
        } else if (root.normalizedDataSeries.length > 0 && root.hasOwnPropertyValue(root.normalizedDataSeries[0], "data") && root.isListLike(root.normalizedDataSeries[0].data)) {
            source = root.normalizedDataSeries[0].data;
        }

        const defaultColors = ["#FF6384", "#36A2EB", "#FFCE56", "#4BC0C0", "#9966FF", "#FF9F40", "#C9CBCF"];
        let result = [];
        for (let i = 0; i < source.length; ++i) {
            const item = source[i];
            if (!root.hasOwnPropertyValue(item, "value"))
                continue;
            result.push({
                label: root.hasOwnPropertyValue(item, "label") ? String(item.label) : "Item " + (i + 1),
                value: Number(item.value),
                color: root.hasOwnPropertyValue(item, "color") ? item.color : defaultColors[i % defaultColors.length]
            });
        }
        return result;
    }
    readonly property real totalValue: {
        let total = 0;
        for (let i = 0; i < effectiveData.length; ++i)
            total += Math.max(0, Number(effectiveData[i].value));
        return total > 0 ? total : 1;
    }
    readonly property bool hasTitleText: title.length > 0 || subtitle.length > 0
    readonly property int hoveredIndex: renderer.hoveredIndex
    readonly property var hoveredDataPoint: renderer.hoveredDataPoint

    signal pointClicked(int index, var dataPoint)
    signal pointHovered(int index, var dataPoint)

    function hasOwnPropertyValue(object, propertyName) {
        return !!object && (typeof object === "object" || typeof object === "function") && propertyName in object;
    }

    function isListLike(value) {
        return !!value && typeof value.length === "number";
    }

    function tooltipText(dataPoint) {
        if (!root.hasOwnPropertyValue(dataPoint, "value"))
            return "";
        const labelText = root.hasOwnPropertyValue(dataPoint, "label") ? String(dataPoint.label) : "";
        const value = Number(dataPoint.value);
        const percent = (value / root.totalValue * 100).toFixed(root.percentPrecision);
        return labelText + ": " + String(value) + " (" + percent + "%)";
    }

    function percentText(dataPoint) {
        if (!root.hasOwnPropertyValue(dataPoint, "value"))
            return "";
        const value = Math.max(0, Number(dataPoint.value));
        return (value / root.totalValue * 100).toFixed(root.percentPrecision) + "%";
    }

    implicitWidth: 600
    implicitHeight: 500

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        visible: root.backgroundVisible
        color: root.backgroundColor
        border.width: 1
        border.color: root.effectiveBorderColor

        layer.enabled: root.shadowEnabled && root.backgroundVisible
        layer.effect: MultiEffect {
            shadowEnabled: root.shadowEnabled
            shadowColor: root.shadowColor
            shadowBlur: 1.0
            shadowHorizontalOffset: 2
            shadowVerticalOffset: 2
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.contentMargins
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
            Layout.minimumHeight: 160

            UTPieChartV2Renderer {
                id: renderer
                dataSeries: root.normalizedDataSeries
                dataPoints: root.normalizedDataPoints
                backgroundColor: root.backgroundColor
                innerRadius: root.innerRadius
                labelsVisible: root.labelsVisible && !root.percentInLegend

                layer {
                    enabled: root.antialiasingSamples > 1
                    samples: root.antialiasingSamples
                    smooth: true
                }

                anchors {
                    fill: parent
                    topMargin: root.tooltipReserve
                    bottomMargin: legend.visible ? legend.implicitHeight + 12 : 0
                }

                onPointClicked: function (index, dataPoint) {
                    root.pointClicked(index, dataPoint);
                }

                onPointHovered: function (index, dataPoint) {
                    root.pointHovered(index, dataPoint);
                }
            }

            Rectangle {
                id: tooltip
                visible: renderer.hoveredIndex >= 0
                opacity: renderer.hoveredIndex >= 0 ? 1 : 0
                x: {
                    let target = renderer.x + renderer.hoverPosition.x + 15;
                    if (target + width > parent.width)
                        target = renderer.x + renderer.hoverPosition.x - width - 10;
                    return Math.max(0, Math.min(target, parent.width - width));
                }
                y: {
                    let target = renderer.y + renderer.hoverPosition.y + 15;
                    if (target + height > parent.height)
                        target = renderer.y + renderer.hoverPosition.y - height - 10;
                    return Math.max(0, Math.min(target, parent.height - height));
                }
                width: Math.max(tooltipColumn.implicitWidth + 20, 96)
                height: tooltipColumn.implicitHeight + 16
                radius: 8
                color: root.tooltipColor
                border.width: 1
                border.color: UTTheme.focusColor
                z: 2

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
                        text: root.hoveredIndex >= 0 && root.hoveredIndex < root.effectiveData.length ? root.effectiveData[root.hoveredIndex].label : ""
                        color: root.tooltipTextColor
                        font.pixelSize: 13
                        font.bold: true
                        textFormat: Text.PlainText
                    }

                    Row {
                        spacing: 5

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: root.hoveredIndex >= 0 && root.hoveredIndex < root.effectiveData.length ? root.effectiveData[root.hoveredIndex].color : UTTheme.focusColor
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        ThemeText {
                            text: {
                                if (root.hoveredIndex >= 0 && root.hoveredIndex < root.effectiveData.length) {
                                    const item = root.effectiveData[root.hoveredIndex];
                                    const value = Number(item.value);
                                    return String(value) + " (" + (value / root.totalValue * 100).toFixed(root.percentPrecision) + "%)";
                                }
                                return "";
                            }
                            color: root.tooltipTextColor
                            font.pixelSize: 13
                            textFormat: Text.PlainText
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            Repeater {
                model: root.labelsVisible && !root.percentInLegend ? renderer.labelItems : []

                ThemeText {
                    required property var modelData

                    x: renderer.x + Number(modelData.x) - width / 2
                    y: renderer.y + Number(modelData.y) - height / 2
                    text: String(modelData.text)
                    color: modelData.color || "#ffffff"
                    font.pixelSize: 12
                    font.bold: true
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    z: 1
                }
            }

            Column {
                anchors.horizontalCenter: renderer.horizontalCenter
                anchors.verticalCenter: renderer.verticalCenter
                visible: root.innerRadius > 0 && (root.centerText.length > 0 || root.centerSubText.length > 0)
                spacing: 2

                ThemeText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.centerText
                    color: root.textColor
                    font.pixelSize: 16
                    font.bold: true
                    textFormat: Text.PlainText
                    visible: text.length > 0
                }

                ThemeText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.centerSubText
                    color: root.subtitleColor
                    font.pixelSize: 12
                    textFormat: Text.PlainText
                    visible: text.length > 0
                }
            }

            Flow {
                id: legend
                visible: root.legendVisible && root.effectiveData.length > 0
                spacing: 6

                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }

                Repeater {
                    model: root.effectiveData

                    Row {
                        id: legendItem
                        required property var modelData
                        width: legend.width >= 180 ? (legend.width - legend.spacing) / 2 : legend.width
                        spacing: 5

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: legendItem.modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        ThemeText {
                            text: legendItem.modelData.label
                            width: Math.max(0, legendItem.width - 10 - legendItem.spacing
                                            - (root.percentInLegend
                                               ? legendItem.spacing + legendPercent.implicitWidth
                                               : 0))
                            color: root.subtitleColor
                            font.pixelSize: 12
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        ThemeText {
                            id: legendPercent
                            visible: root.percentInLegend
                            text: root.percentText(legendItem.modelData)
                            color: root.textColor
                            font.pixelSize: 12
                            font.bold: true
                            textFormat: Text.PlainText
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
}
