/*
 * @file UTPieChart.qml
 * @brief 饼图/环形图组件，支持数据系列、扇区悬浮、点击交互和图例
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
    property string title: "Pie Chart"
    property string subtitle: "数据占比分析"

    /// 多系列数据格式; 饼图取第一个系列的 data
    property var dataSeries: [
        {
            name: "Distribution",
            data: [
                {
                    label: "Category A",
                    value: 30,
                    color: "#FF6384"
                },
                {
                    label: "Category B",
                    value: 20,
                    color: "#36A2EB"
                },
                {
                    label: "Category C",
                    value: 15,
                    color: "#FFCE56"
                },
                {
                    label: "Category D",
                    value: 25,
                    color: "#4BC0C0"
                },
                {
                    label: "Category E",
                    value: 10,
                    color: "#9966FF"
                }
            ]
        }
    ]

    /// 兼容单数据点数组格式
    property var dataPoints: []

    /// 合并后的有效数据
    readonly property var effectiveData: {
        var data = [];
        if (dataPoints && dataPoints.length > 0) {
            data = dataPoints;
        } else if (dataSeries && dataSeries.length > 0) {
            data = dataSeries[0].data;
        }
        var defaultColors = ["#FF6384", "#36A2EB", "#FFCE56", "#4BC0C0", "#9966FF", "#FF9F40", "#C9CBCF"];
        var result = [];
        for (var i = 0; i < data.length; i++) {
            var item = data[i];
            result.push({
                label: item.label || ("Item " + i),
                value: item.value,
                color: item.color || defaultColors[i % defaultColors.length]
            });
        }
        return result;
    }

    property color tooltipColor: UTTheme.backgroundColor
    property color tooltipTextColor: UTTheme.primaryTextColor
    property int hoveredIndex: -1

    /// 饼图特有: innerRadius > 0 时为环形图 (Donut)
    property real innerRadius: 0
    /// 环形图中心文字 (仅 innerRadius > 0 时显示)
    property string centerText: ""
    /// 环形图中心副文字
    property string centerSubText: ""
    property bool showLabels: true
    /// 百分比小数位精度，默认2位
    property int percentPrecision: 2
    /// 是否显示底部图例
    property bool showLegend: true
    /// true 时百分比显示在图例中；false 时恢复扇区内显示
    property bool percentInLegend: true

    signal pointClicked(int index, var dataPoint)
    signal pointHovered(int index, var dataPoint)

    // =====================================================================
    // 样式属性
    // =====================================================================
    property bool backgroundVisible: true
    property real radius: UTTheme.radius || 8
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

    // =====================================================================
    // 计算属性
    // =====================================================================
    property real totalValue: {
        var total = 0;
        var data = root.effectiveData;
        for (var i = 0; i < data.length; i++) {
            total += data[i].value;
        }
        return total > 0 ? total : 1;
    }

    function percentText(dataPoint) {
        if (!dataPoint || dataPoint.value === undefined)
            return "";
        const value = Math.max(0, Number(dataPoint.value));
        return (value / root.totalValue * 100).toFixed(root.percentPrecision) + "%";
    }

    // =====================================================================
    // 背景与阴影
    // =====================================================================
    Rectangle {
        id: background
        anchors.fill: parent
        radius: root.radius
        color: root.backgroundVisible ? root.backgroundColor : "transparent"
        border.color: root.backgroundVisible ? Qt.rgba(UTTheme.borderColor.r || 0.84, UTTheme.borderColor.g || 0.86, UTTheme.borderColor.b || 0.89, 0.4) : "transparent"
        border.width: root.backgroundVisible ? 1 : 0

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
    // 动画控制器 — 扇区 hover 偏移量
    // =====================================================================
    Item {
        id: animationController
        visible: false

        Repeater {
            id: sliceAnimators
            model: root.effectiveData.length

            Item {
                property real targetOffset: index === root.hoveredIndex ? 10 : 0
                property real currentOffset: targetOffset

                Behavior on currentOffset {
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }

                onCurrentOffsetChanged: chartCanvas.requestPaint()
            }
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
        anchors.bottomMargin: legend.visible ? legend.implicitHeight + root.chartPadding : root.chartPadding

        Canvas {
            id: chartCanvas
            anchors.fill: parent

            property var sliceRects: []

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);

                var data = root.effectiveData;
                if (data.length === 0)
                    return;

                var centerX = width / 2;
                var centerY = height / 2;
                var maxRadius = Math.min(width, height) / 2 - 20;
                if (maxRadius <= 0)
                    return;

                var currentAngle = -Math.PI / 2; // 从 12 点钟方向开始
                var total = root.totalValue;

                sliceRects = [];

                for (var i = 0; i < data.length; i++) {
                    var item = data[i];
                    var sliceAngle = (item.value / total) * (Math.PI * 2);
                    var endAngle = currentAngle + sliceAngle;

                    var r = maxRadius;
                    var animator = sliceAnimators.itemAt(i);
                    if (animator) {
                        r += animator.currentOffset;
                    }

                    // 绘制扇区
                    ctx.beginPath();
                    ctx.moveTo(centerX, centerY);
                    if (root.innerRadius > 0) {
                        ctx.arc(centerX, centerY, r, currentAngle, endAngle, false);
                        ctx.arc(centerX, centerY, root.innerRadius, endAngle, currentAngle, true);
                    } else {
                        ctx.arc(centerX, centerY, r, currentAngle, endAngle, false);
                        ctx.lineTo(centerX, centerY);
                    }
                    ctx.closePath();

                    ctx.fillStyle = item.color;
                    ctx.fill();
                    ctx.strokeStyle = root.backgroundColor;
                    ctx.lineWidth = 2;
                    ctx.stroke();

                    sliceRects.push({
                        startAngle: currentAngle,
                        endAngle: endAngle,
                        innerRadius: root.innerRadius,
                        outerRadius: r,
                        index: i,
                        data: item
                    });

                    // 扇区标签
                    if (root.showLabels && !root.percentInLegend && sliceAngle > 0.1) {
                        var midAngle = currentAngle + sliceAngle / 2;
                        var labelRadius = root.innerRadius > 0 ? (r + root.innerRadius) / 2 : r * 0.7;
                        var lx = centerX + Math.cos(midAngle) * labelRadius;
                        var ly = centerY + Math.sin(midAngle) * labelRadius;

                        ctx.fillStyle = "white";
                        ctx.font = "bold 12px sans-serif";
                        ctx.textAlign = "center";
                        ctx.textBaseline = "middle";

                        var percent = ((item.value / total) * 100).toFixed(root.percentPrecision) + "%";
                        ctx.fillText(percent, lx, ly);
                    }

                    currentAngle = endAngle;
                }

                // 环形图中心文字
                if (root.innerRadius > 0 && root.centerText !== "") {
                    ctx.fillStyle = root.textColor;
                    ctx.font = "bold 16px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "middle";
                    if (root.centerSubText !== "") {
                        ctx.fillText(root.centerText, centerX, centerY - 8);
                        ctx.font = "12px sans-serif";
                        ctx.fillStyle = root.subtitleColor;
                        ctx.fillText(root.centerSubText, centerX, centerY + 12);
                    } else {
                        ctx.fillText(root.centerText, centerX, centerY);
                    }
                }
            }

            Connections {
                target: root
                function onDataPointsChanged() {
                    chartCanvas.requestPaint();
                }
                function onDataSeriesChanged() {
                    chartCanvas.requestPaint();
                }
                function onEffectiveDataChanged() {
                    chartCanvas.requestPaint();
                }
                function onBackgroundColorChanged() {
                    chartCanvas.requestPaint();
                }
                function onShowLabelsChanged() {
                    chartCanvas.requestPaint();
                }
                function onPercentInLegendChanged() {
                    chartCanvas.requestPaint();
                }
            }
        }

        // =================================================================
        // 交互层
        // =================================================================
        MouseArea {
            id: interactionArea
            anchors.fill: parent
            hoverEnabled: true

            onPositionChanged: function (mouse) {
                var slices = chartCanvas.sliceRects;
                var centerX = chartCanvas.width / 2;
                var centerY = chartCanvas.height / 2;
                var dx = mouse.x - centerX;
                var dy = mouse.y - centerY;
                var dist = Math.sqrt(dx * dx + dy * dy);
                var angle = Math.atan2(dy, dx);

                var found = false;
                for (var i = 0; i < slices.length; i++) {
                    var s = slices[i];
                    if (dist >= s.innerRadius && dist <= s.outerRadius) {
                        var mouseA = angle;
                        if (mouseA < -Math.PI / 2)
                            mouseA += Math.PI * 2;
                        if (mouseA >= s.startAngle && mouseA < s.endAngle) {
                            if (root.hoveredIndex !== s.index) {
                                root.hoveredIndex = s.index;
                                root.pointHovered(s.index, s.data);
                            }
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    root.hoveredIndex = -1;
                }
            }

            onExited: function () {
                root.hoveredIndex = -1;
            }

            onClicked: function (mouse) {
                if (root.hoveredIndex >= 0) {
                    var data = root.effectiveData[root.hoveredIndex];
                    root.pointClicked(root.hoveredIndex, data);
                }
            }
        }

        // =================================================================
        // Tooltip
        // =================================================================
        Rectangle {
            id: tooltip
            visible: root.hoveredIndex >= 0
            opacity: root.hoveredIndex >= 0 ? 1 : 0
            width: Math.max(tooltipContent.implicitWidth + 20, 96)
            height: tooltipContent.implicitHeight + 16
            radius: 8
            color: root.tooltipColor
            border.color: UTTheme.focusColor
            border.width: 1

            x: {
                var pos = interactionArea.mouseX + 15;
                if (pos + width > chartArea.width)
                    return Math.max(0, interactionArea.mouseX - width - 10);
                return Math.max(0, pos);
            }
            y: {
                var pos = interactionArea.mouseY + 15;
                if (pos + height > chartArea.height)
                    return Math.max(0, interactionArea.mouseY - height - 10);
                return Math.max(0, pos);
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
                    text: root.hoveredIndex >= 0 ? root.effectiveData[root.hoveredIndex].label : ""
                    color: root.tooltipTextColor
                    font.pixelSize: root.fontSize - 1
                    font.bold: true
                    font.family: UTTheme.fontFamily
                }

                Row {
                    spacing: 5

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: root.hoveredIndex >= 0 ? root.effectiveData[root.hoveredIndex].color : UTTheme.focusColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: {
                            if (root.hoveredIndex >= 0) {
                                var item = root.effectiveData[root.hoveredIndex];
                                return item.value + " (" + (item.value / root.totalValue * 100).toFixed(root.percentPrecision) + "%)";
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
    // 图例 (Legend)
    // =====================================================================
    Flow {
        id: legend
        visible: root.showLegend
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12
        anchors.bottomMargin: 8
        spacing: 6

        Repeater {
            model: root.effectiveData

            Row {
                id: legendItem
                required property var modelData
                width: legend.width >= 180 ? (legend.width - legend.spacing) / 2 : legend.width
                spacing: 5

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: legendItem.modelData.color
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: legendItem.modelData.label
                    width: Math.max(0, legendItem.width - 12 - legendItem.spacing
                                    - (root.percentInLegend
                                       ? legendItem.spacing + legendPercent.implicitWidth
                                       : 0))
                    color: root.textColor
                    font.pixelSize: 12
                    font.family: UTTheme.fontFamily
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: legendPercent
                    visible: root.percentInLegend
                    text: root.percentText(legendItem.modelData)
                    color: root.textColor
                    font.pixelSize: 12
                    font.bold: true
                    font.family: UTTheme.fontFamily
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
