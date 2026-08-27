/*
 * @file UTAreaChart.qml
 * @brief 面积图/折线图组件，支持平滑/直线/阶梯三种线型、多系列叠加和悬浮提示
 */

// ========================================================================
// ========================================================================

pragma ComponentBehavior: Bound

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
    // 线型枚举
    // =====================================================================
    enum LineStyle {
        Smooth, ///< 平滑曲线
        Linear,  ///< 直线连接
        Step     ///< 阶梯连接
    }

    // =====================================================================
    // 接口属性 & 信号
    // =====================================================================
    property string title: "Area Chart"
    property string subtitle: "以下是前六个月用户访问数"

    /// 多数据系列
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

    property int lineStyle: UTAreaChart.LineStyle.Smooth
    property color areaColor: Qt.rgba(UTTheme.focusColor.r, UTTheme.focusColor.g, UTTheme.focusColor.b, 0.3)
    property color lineColor: UTTheme.focusColor
    property color tooltipColor: UTTheme.backgroundColor
    property color tooltipTextColor: UTTheme.primaryTextColor
    property int hoveredIndex: -1

    signal pointClicked(int index, var dataPoint)
    signal pointHovered(int index, var dataPoint)

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
        return Math.max(1, max);
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
    // 线型切换按钮
    // =====================================================================
    Rectangle {
        id: styleButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 40
        width: 80
        height: 32
        radius: 16
        color: root.backgroundVisible ? UTTheme.surfaceColor : "transparent"
        border.color: UTTheme.borderColor
        border.width: root.backgroundVisible ? 1 : 0

        Text {
            anchors.centerIn: parent
            font.family: UTTheme.fontFamily
            text: {
                switch (root.lineStyle) {
                case UTAreaChart.LineStyle.Smooth:
                    return "Smooth";
                case UTAreaChart.LineStyle.Linear:
                    return "Linear";
                case UTAreaChart.LineStyle.Step:
                    return "Step";
                default:
                    return "Smooth";
                }
            }
            font.pixelSize: 12
            color: root.textColor
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: UTTheme.focusColor
            opacity: styleButton.hovered ? 0.1 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }
        }

        property bool hovered: false

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: parent.hovered = true
            onExited: parent.hovered = false
            onClicked: {
                switch (root.lineStyle) {
                case UTAreaChart.LineStyle.Smooth:
                    root.lineStyle = UTAreaChart.LineStyle.Linear;
                    break;
                case UTAreaChart.LineStyle.Linear:
                    root.lineStyle = UTAreaChart.LineStyle.Step;
                    break;
                case UTAreaChart.LineStyle.Step:
                    root.lineStyle = UTAreaChart.LineStyle.Smooth;
                    break;
                }
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

        Canvas {
            id: chartCanvas
            anchors.fill: parent
            anchors.margins: 8
            anchors.bottomMargin: legend.visible ? 100 : 40

            // -------------------------------------------------------------
            // 绘制面积路径 (填充用)
            // -------------------------------------------------------------
            function drawAreaPath(ctx, points, lineStyle) {
                switch (lineStyle) {
                case UTAreaChart.LineStyle.Linear:
                    for (var i = 1; i < points.length; i++) {
                        ctx.lineTo(points[i].x, points[i].y);
                    }
                    break;
                case UTAreaChart.LineStyle.Step:
                    for (var j = 1; j < points.length; j++) {
                        ctx.lineTo(points[j].x, points[j - 1].y);
                        ctx.lineTo(points[j].x, points[j].y);
                    }
                    break;
                case UTAreaChart.LineStyle.Smooth:
                default:
                    if (points.length > 2) {
                        var cp1x = points[0].x + (points[1].x - points[0].x) * 0.25;
                        var cp1y = points[0].y;
                        var cp2x = points[1].x - (points[2].x - points[0].x) * 0.25;
                        var cp2y = points[1].y - (points[2].y - points[0].y) * 0.25;
                        ctx.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, points[1].x, points[1].y);
                        for (var k = 2; k < points.length - 1; k++) {
                            var p0 = points[k - 2], p1 = points[k - 1];
                            var p2 = points[k], p3 = points[k + 1];
                            var c1x = p1.x + (p2.x - p0.x) / 6;
                            var c1y = p1.y + (p2.y - p0.y) / 6;
                            var c2x = p2.x - (p3.x - p1.x) / 6;
                            var c2y = p2.y - (p3.y - p1.y) / 6;
                            ctx.bezierCurveTo(c1x, c1y, c2x, c2y, p2.x, p2.y);
                        }
                        if (points.length > 2) {
                            var last = points.length - 1;
                            var lc1x = points[last - 1].x + (points[last].x - points[last - 2].x) * 0.25;
                            var lc1y = points[last - 1].y + (points[last].y - points[last - 2].y) * 0.25;
                            var lc2x = points[last].x - (points[last].x - points[last - 1].x) * 0.25;
                            var lc2y = points[last].y;
                            ctx.bezierCurveTo(lc1x, lc1y, lc2x, lc2y, points[last].x, points[last].y);
                        }
                    } else if (points.length === 2) {
                        var mx = (points[0].x + points[1].x) / 2;
                        var my = (points[0].y + points[1].y) / 2;
                        ctx.quadraticCurveTo(mx, points[0].y, points[1].x, points[1].y);
                    } else {
                        ctx.lineTo(points[0].x, points[0].y);
                    }
                    break;
                }
            }

            // -------------------------------------------------------------
            // 绘制线条路径 (描边用)
            // -------------------------------------------------------------
            function drawLinePath(ctx, points, lineStyle) {
                switch (lineStyle) {
                case UTAreaChart.LineStyle.Linear:
                    for (var i = 1; i < points.length; i++) {
                        ctx.lineTo(points[i].x, points[i].y);
                    }
                    break;
                case UTAreaChart.LineStyle.Step:
                    for (var j = 1; j < points.length; j++) {
                        ctx.lineTo(points[j].x, points[j - 1].y);
                        ctx.lineTo(points[j].x, points[j].y);
                    }
                    break;
                case UTAreaChart.LineStyle.Smooth:
                default:
                    if (points.length > 2) {
                        var cp1x = points[0].x + (points[1].x - points[0].x) * 0.25;
                        var cp1y = points[0].y;
                        var cp2x = points[1].x - (points[2].x - points[0].x) * 0.25;
                        var cp2y = points[1].y - (points[2].y - points[0].y) * 0.25;
                        ctx.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, points[1].x, points[1].y);
                        for (var k = 2; k < points.length - 1; k++) {
                            var p0 = points[k - 2], p1 = points[k - 1];
                            var p2 = points[k], p3 = points[k + 1];
                            var c1x = p1.x + (p2.x - p0.x) / 6;
                            var c1y = p1.y + (p2.y - p0.y) / 6;
                            var c2x = p2.x - (p3.x - p1.x) / 6;
                            var c2y = p2.y - (p3.y - p1.y) / 6;
                            ctx.bezierCurveTo(c1x, c1y, c2x, c2y, p2.x, p2.y);
                        }
                        if (points.length > 2) {
                            var last = points.length - 1;
                            var lc1x = points[last - 1].x + (points[last].x - points[last - 2].x) * 0.25;
                            var lc1y = points[last - 1].y + (points[last].y - points[last - 2].y) * 0.25;
                            var lc2x = points[last].x - (points[last].x - points[last - 1].x) * 0.25;
                            var lc2y = points[last].y;
                            ctx.bezierCurveTo(lc1x, lc1y, lc2x, lc2y, points[last].x, points[last].y);
                        }
                    } else if (points.length === 2) {
                        var mx = (points[0].x + points[1].x) / 2;
                        var my = (points[0].y + points[1].y) / 2;
                        ctx.quadraticCurveTo(mx, points[0].y, points[1].x, points[1].y);
                    } else if (points.length === 1) {
                        // 单点不画线
                    } else {
                        for (var l = 1; l < points.length; l++) {
                            ctx.lineTo(points[l].x, points[l].y);
                        }
                    }
                    break;
                }
            }

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);

                var series = root.effectiveDataSeries;
                if (series.length === 0 || series[0].data.length === 0)
                    return;

                var dataLength = series[0].data.length;
                var stepX = width / dataLength;
                var chartH = height - 16;

                for (var si = 0; si < series.length; si++) {
                    var curSeries = series[si];
                    var data = curSeries.data;

                    var points = [];
                    for (var i = 0; i < data.length; i++) {
                        var px = stepX * i + stepX / 2;
                        var py = 8 + chartH - (data[i].value / root.maxValue) * chartH;
                        points.push({
                            x: px,
                            y: py
                        });
                    }

                    // 填充面积
                    ctx.beginPath();
                    ctx.moveTo(points[0].x, 8 + chartH);
                    ctx.lineTo(points[0].x, points[0].y);
                    drawAreaPath(ctx, points, root.lineStyle);
                    ctx.lineTo(points[points.length - 1].x, 8 + chartH);
                    ctx.closePath();

                    var sc = curSeries.color;
                    var color = Qt.color(sc);
                    ctx.fillStyle = Qt.rgba(color.r, color.g, color.b, 0.3);
                    ctx.fill();

                    // 描边
                    ctx.beginPath();
                    ctx.moveTo(points[0].x, points[0].y);
                    drawLinePath(ctx, points, root.lineStyle);
                    ctx.strokeStyle = sc;
                    ctx.lineWidth = 2;
                    ctx.stroke();

                    // 数据点 (hover 时高亮)
                    if (root.hoveredIndex >= 0 && root.hoveredIndex < points.length) {
                        var hp = points[root.hoveredIndex];
                        ctx.beginPath();
                        ctx.arc(hp.x, hp.y, 5, 0, 2 * Math.PI);
                        ctx.fillStyle = sc;
                        ctx.fill();
                        ctx.beginPath();
                        ctx.arc(hp.x, hp.y, 3, 0, 2 * Math.PI);
                        ctx.fillStyle = "white";
                        ctx.fill();
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
                function onAreaColorChanged() {
                    chartCanvas.requestPaint();
                }
                function onLineColorChanged() {
                    chartCanvas.requestPaint();
                }
                function onHoveredIndexChanged() {
                    chartCanvas.requestPaint();
                }
                function onLineStyleChanged() {
                    chartCanvas.requestPaint();
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
                var series = root.effectiveDataSeries;
                if (series.length === 0 || series[0].data.length === 0)
                    return;

                var dataLength = series[0].data.length;
                var stepX = chartCanvas.width / dataLength;
                var index = Math.floor(mouse.x / stepX);
                index = Math.max(0, Math.min(index, dataLength - 1));

                var baseX = root.chartPadding + index * stepX + stepX / 2;
                var targetX = Math.max(10, Math.min(baseX - tooltip.width / 2, root.width - tooltip.width - 10));

                var dataValue = series[0].data[index].value;
                var dataY = root.topPadding + root.chartHeight - (dataValue / root.maxValue) * root.chartHeight;
                var targetY = Math.max(10, dataY - tooltip.height - 10);

                if (root.hoveredIndex === -1) {
                    tooltip.x = targetX;
                    tooltip.y = targetY;
                } else {
                    tooltip.x = targetX;
                    tooltip.y = targetY;
                }

                if (index !== root.hoveredIndex) {
                    root.hoveredIndex = index;
                    root.pointHovered(index, series[0].data[index]);
                }
            }

            onExited: function () {
                root.hoveredIndex = -1;
            }

            onClicked: function (mouse) {
                if (root.hoveredIndex >= 0) {
                    var series = root.effectiveDataSeries;
                    if (series.length > 0 && series[0].data.length > root.hoveredIndex) {
                        root.pointClicked(root.hoveredIndex, series[0].data[root.hoveredIndex]);
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
            model: root.effectiveDataSeries.length > 0 ? root.effectiveDataSeries[0].data : []

            delegate: Text {
                required property var modelData
                width: root.chartWidth / (root.effectiveDataSeries.length > 0 ? root.effectiveDataSeries[0].data.length : 1)
                text: modelData.month
                font.pixelSize: root.fontSize - 2
                color: root.subtitleColor
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // =====================================================================
    // Tooltip
    // =====================================================================
    Rectangle {
        id: tooltip
        opacity: root.hoveredIndex >= 0 ? 1 : 0
        visible: root.hoveredIndex >= 0

        width: tooltipContent.implicitWidth + 20
        height: tooltipContent.implicitHeight + 16
        radius: 8
        color: root.tooltipColor
        border.color: UTTheme.focusColor
        border.width: 1

        x: 0
        y: 0

        Behavior on x {
            enabled: root.hoveredIndex >= 0
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
        Behavior on y {
            enabled: root.hoveredIndex >= 0
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
                text: root.hoveredIndex >= 0 && root.effectiveDataSeries.length > 0 ? root.effectiveDataSeries[0].data[root.hoveredIndex].label : ""
                font.pixelSize: root.fontSize - 1
                font.family: UTTheme.fontFamily
                color: root.tooltipTextColor
                font.bold: true
            }

            Repeater {
                model: root.effectiveDataSeries

                Row {
                    required property var modelData
                    spacing: 5

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: modelData.color
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: modelData.name
                        font.pixelSize: root.fontSize - 2
                        font.family: UTTheme.fontFamily
                        color: root.subtitleColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: root.hoveredIndex >= 0 && modelData.data.length > root.hoveredIndex ? modelData.data[root.hoveredIndex].value : ""
                        font.pixelSize: root.fontSize - 1
                        font.family: UTTheme.fontFamily
                        color: root.tooltipTextColor
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    // =====================================================================
    // 底部图例
    // =====================================================================
    Row {
        id: legend
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 20
        spacing: 30
        visible: root.effectiveDataSeries.length > 1 || (root.effectiveDataSeries.length === 1 && root.effectiveDataSeries[0].name !== "Data")

        Repeater {
            model: root.effectiveDataSeries

            Row {
                required property var modelData
                spacing: 8
                Rectangle {
                    width: 12
                    height: 12
                    radius: 2
                    color: modelData.color
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: modelData.name
                    font.pixelSize: root.fontSize - 2
                        font.family: UTTheme.fontFamily
                    color: root.subtitleColor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
