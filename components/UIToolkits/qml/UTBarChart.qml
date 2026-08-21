/*
 * @file UTBarChart.qml
 * @brief 柱状图组件，支持多数据系列分组、悬浮提示和点击交互
 */

// ========================================================================
// ========================================================================

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Effects
import UIToolkits

Rectangle {
    id: root

    implicitWidth: 600
    implicitHeight: 500
    color: "transparent"
    clip: false

    // =====================================================================
    // 内部主题引用
    // =====================================================================

    // =====================================================================
    // 接口属性 & 信号
    // =====================================================================
    property string title: "Bar Chart"
    property string subtitle: "数据统计概览"

    /// 多数据系列
    property var dataSeries: [
        {
            name: "Sales",
            color: UTTheme.focusColor,
            data: [
                {
                    label: "Jan",
                    value: 120
                },
                {
                    label: "Feb",
                    value: 180
                },
                {
                    label: "Mar",
                    value: 237
                },
                {
                    label: "Apr",
                    value: 160
                },
                {
                    label: "May",
                    value: 90
                },
                {
                    label: "Jun",
                    value: 200
                }
            ]
        }
    ]

    /// 兼容单数据点数组格式
    property var dataPoints: []

    /// 合并后的有效数据系列
    readonly property var effectiveDataSeries: {
        if (dataPoints && dataPoints.length > 0) {
            return [
                {
                    name: "Data",
                    color: UTTheme.focusColor,
                    data: dataPoints
                }
            ];
        } else {
            return dataSeries;
        }
    }

    property color barColor: UTTheme.focusColor
    property color tooltipColor: UTTheme.backgroundColor
    property color tooltipTextColor: UTTheme.primaryTextColor
    property int hoveredSeriesIndex: -1
    property int hoveredDataIndex: -1

    function scheduleChartRepaint() {
        chartRepaintTimer.restart();
    }

    Timer {
        id: chartRepaintTimer
        interval: 0
        repeat: false
        onTriggered: chartCanvas.requestPaint()
    }

    signal pointClicked(int seriesIndex, int dataIndex, var dataPoint)
    signal pointHovered(int seriesIndex, int dataIndex, var dataPoint)

    // =====================================================================
    // 样式属性
    // =====================================================================
    property bool backgroundVisible: true
    property real radius: 20
    property int fontSize: 14
    property int titleFontSize: 18
    property int subtitleFontSize: 12
    property color backgroundColor: UTTheme.surfaceColor
    property color textColor: UTTheme.primaryTextColor
    property color subtitleColor: UTTheme.primaryTextColor
    property bool shadowEnabled: true
    property color shadowColor: UTTheme.shadowColor
    property int chartPadding: 20
    property int topPadding: 90
    property real barSpacing: 0.2   ///< 柱间距比例 (0-1)
    property real groupSpacing: 0.3 ///< 组间距比例 (0-1)

    // =====================================================================
    // 计算属性
    // =====================================================================
    property real maxValue: {
        var max = 0;
        var series = root.effectiveDataSeries;
        for (var s = 0; s < series.length; s++) {
            var data = series[s].data;
            for (var i = 0; i < data.length; i++) {
                if (data[i].value > max) {
                    max = data[i].value;
                }
            }
        }
        return max > 0 ? max : 100;
    }

    property real chartWidth: width - chartPadding * 2
    property real chartHeight: height - topPadding - chartPadding - (legend.visible ? 60 : 0)

    // =====================================================================
    // 背景与阴影
    // =====================================================================
    Rectangle {
        id: background
        anchors.fill: parent
        radius: root.radius
        color: root.backgroundVisible ? root.backgroundColor : "transparent"

        layer.enabled: root.shadowEnabled && root.backgroundVisible
        layer.effect: MultiEffect {
            shadowEnabled: root.shadowEnabled
            shadowColor: root.shadowColor
            shadowBlur: 1.0
            shadowHorizontalOffset: 2
            shadowVerticalOffset: 2
        }
    }

    // =====================================================================
    // 标题区域
    // =====================================================================
    Column {
        id: titleColumn
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 40
        spacing: 5

        Text {
            text: root.title
            font.pixelSize: root.titleFontSize
            font.bold: true
            font.family: UTTheme.fontFamily
            color: root.textColor
        }

        Text {
            text: root.subtitle
            font.pixelSize: root.subtitleFontSize
            font.family: UTTheme.fontFamily
            color: root.subtitleColor
        }
    }

    // =====================================================================
    // 图表区域
    // =====================================================================
    Item {
        id: chartArea
        anchors.fill: parent
        anchors.topMargin: root.topPadding
        anchors.margins: root.chartPadding

        Canvas {
            id: chartCanvas
            anchors.fill: parent
            anchors.margins: 8
            anchors.bottomMargin: legend.visible ? 100 : 40

            property var barRects: []

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);

                var series = root.effectiveDataSeries;
                if (series.length === 0 || series[0].data.length === 0)
                    return;

                var dataLength = series[0].data.length;
                var seriesCount = series.length;
                var chartH = height;

                var groupTotalWidth = width / dataLength;
                var groupWidth = groupTotalWidth * (1 - root.groupSpacing);
                var barWidth = (groupWidth / seriesCount) * (1 - (seriesCount > 1 ? root.barSpacing : 0));

                var groupOffsetX = (groupTotalWidth - groupWidth) / 2;
                var barStepX = groupWidth / seriesCount;
                var barOffsetX = (barStepX - barWidth) / 2;

                barRects = [];

                // 参考网格线
                ctx.beginPath();
                ctx.strokeStyle = Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.1);
                ctx.lineWidth = 1;
                for (var j = 0; j <= 5; j++) {
                    var lineY = chartH * (j / 5);
                    ctx.moveTo(0, lineY);
                    ctx.lineTo(width, lineY);
                }
                ctx.stroke();

                // 绘制柱子
                for (var i = 0; i < dataLength; i++) {
                    var groupX = i * groupTotalWidth + groupOffsetX;

                    for (var s = 0; s < seriesCount; s++) {
                        var data = series[s].data[i];
                        var val = data.value;
                        var barHeight = (val / root.maxValue) * chartH;
                        var x = groupX + s * barStepX + barOffsetX;
                        var y = chartH - barHeight;

                        barRects.push({
                            x: x,
                            y: y,
                            w: barWidth,
                            h: barHeight,
                            seriesIndex: s,
                            dataIndex: i,
                            value: val,
                            label: data.label
                        });

                        ctx.fillStyle = series[s].color;
                        if (root.hoveredSeriesIndex === s && root.hoveredDataIndex === i) {
                            ctx.globalAlpha = 0.8;
                        } else {
                            ctx.globalAlpha = 1.0;
                        }

                        var r = Math.min(barWidth / 2, 5);
                        ctx.beginPath();
                        ctx.moveTo(x + r, y);
                        ctx.lineTo(x + barWidth - r, y);
                        ctx.quadraticCurveTo(x + barWidth, y, x + barWidth, y + r);
                        ctx.lineTo(x + barWidth, y + barHeight);
                        ctx.lineTo(x, y + barHeight);
                        ctx.lineTo(x, y + r);
                        ctx.quadraticCurveTo(x, y, x + r, y);
                        ctx.closePath();
                        ctx.fill();
                    }
                }
                ctx.globalAlpha = 1.0;
            }

            Connections {
                target: root
                function onDataPointsChanged() {
                    root.scheduleChartRepaint();
                }
                function onDataSeriesChanged() {
                    root.scheduleChartRepaint();
                }
                function onEffectiveDataSeriesChanged() {
                    root.scheduleChartRepaint();
                }
                function onTextColorChanged() {
                    root.scheduleChartRepaint();
                }
                function onHoveredSeriesIndexChanged() {
                    root.scheduleChartRepaint();
                }
                function onHoveredDataIndexChanged() {
                    root.scheduleChartRepaint();
                }
            }
        }

        // =================================================================
        // 交互层
        // =================================================================
        MouseArea {
            id: hoverMouseArea
            anchors.fill: parent
            hoverEnabled: true

            onPositionChanged: function (mouse) {
                var rects = chartCanvas.barRects;
                var found = false;
                var canvasX = mouse.x - chartCanvas.x;
                var canvasY = mouse.y - chartCanvas.y;

                for (var i = 0; i < rects.length; i++) {
                    var r = rects[i];
                    if (canvasX >= r.x && canvasX <= r.x + r.w && canvasY >= r.y && canvasY <= r.y + r.h) {
                        var tooltipX = chartCanvas.x + r.x + r.w / 2 - tooltip.width / 2;
                        var tooltipY = chartCanvas.y + r.y - tooltip.height - 5;
                        var targetX = Math.max(0, Math.min(tooltipX, root.width - tooltip.width));
                        var targetY = tooltipY;

                        if (root.hoveredSeriesIndex === -1) {
                            tooltip.x = targetX;
                            tooltip.y = targetY;
                        } else {
                            tooltip.x = targetX;
                            tooltip.y = targetY;
                        }

                        if (root.hoveredSeriesIndex !== r.seriesIndex || root.hoveredDataIndex !== r.dataIndex) {
                            root.hoveredSeriesIndex = r.seriesIndex;
                            root.hoveredDataIndex = r.dataIndex;
                            root.pointHovered(r.seriesIndex, r.dataIndex, root.effectiveDataSeries[r.seriesIndex].data[r.dataIndex]);
                        }

                        found = true;
                        break;
                    }
                }

                if (!found) {
                    root.hoveredSeriesIndex = -1;
                    root.hoveredDataIndex = -1;
                }
            }

            onExited: function () {
                root.hoveredSeriesIndex = -1;
                root.hoveredDataIndex = -1;
            }

            onClicked: function (mouse) {
                if (root.hoveredSeriesIndex >= 0 && root.hoveredDataIndex >= 0) {
                    var series = root.effectiveDataSeries;
                    var data = series[root.hoveredSeriesIndex].data[root.hoveredDataIndex];
                    root.pointClicked(root.hoveredSeriesIndex, root.hoveredDataIndex, data);
                }
            }
        }

        // =================================================================
        // Tooltip
        // =================================================================
        Rectangle {
            id: tooltip
            opacity: (root.hoveredSeriesIndex >= 0 && root.hoveredDataIndex >= 0) ? 1 : 0
            visible: root.hoveredSeriesIndex >= 0 && root.hoveredDataIndex >= 0

            width: Math.max(tooltipContent.implicitWidth + 20, 96)
            height: tooltipContent.implicitHeight + 16
            radius: 8
            color: root.tooltipColor
            border.color: UTTheme.focusColor
            border.width: 1

            x: 0
            y: 0

            Behavior on x {
                enabled: root.hoveredSeriesIndex >= 0
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on y {
                enabled: root.hoveredSeriesIndex >= 0
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
                id: tooltipContent
                x: 10
                y: 8
                spacing: 4

                Text {
                    text: root.hoveredSeriesIndex >= 0 && root.effectiveDataSeries[root.hoveredSeriesIndex].name ? root.effectiveDataSeries[root.hoveredSeriesIndex].name : ""
                    color: root.tooltipTextColor
                    font.pixelSize: root.fontSize - 1
                    font.bold: true
                    font.family: UTTheme.fontFamily
                    visible: text.length > 0
                }

                Row {
                    spacing: 5

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: root.hoveredSeriesIndex >= 0 ? root.effectiveDataSeries[root.hoveredSeriesIndex].color : root.barColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: {
                            if (root.hoveredSeriesIndex >= 0 && root.hoveredDataIndex >= 0) {
                                var series = root.effectiveDataSeries[root.hoveredSeriesIndex];
                                var item = series.data[root.hoveredDataIndex];
                                return (item.label || "") + ": " + item.value;
                            }
                            return "";
                        }
                        color: root.tooltipTextColor
                        font.pixelSize: root.fontSize - 1
                        font.family: UTTheme.fontFamily
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    // =====================================================================
    // X 轴标签
    // =====================================================================
    Row {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.chartPadding
        anchors.bottomMargin: legend.visible ? 75 : 15

        Repeater {
            model: root.effectiveDataSeries.length > 0 ? root.effectiveDataSeries[0].data.length : 0

            Item {
                width: (root.width - root.chartPadding * 2) / (root.effectiveDataSeries[0].data.length || 1)
                height: 20

                Text {
                    anchors.centerIn: parent
                    text: {
                        var series = root.effectiveDataSeries;
                        if (series.length > 0 && series[0].data.length > index) {
                            var item = series[0].data[index];
                            return item.label || item.month || "";
                        }
                        return "";
                    }
                    font.pixelSize: 10
                    font.family: UTTheme.fontFamily
                    color: root.textColor
                    elide: Text.ElideRight
                }
            }
        }
    }

    // =====================================================================
    // 图例 (Legend)
    // =====================================================================
    Row {
        id: legend
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 20
        spacing: 20
        visible: root.effectiveDataSeries.length > 1

        Repeater {
            model: root.effectiveDataSeries

            Row {
                spacing: 5
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: modelData.color
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: modelData.name
                    color: root.textColor
                    font.pixelSize: 12
                    font.family: UTTheme.fontFamily
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
