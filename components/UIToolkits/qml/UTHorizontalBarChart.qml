/*
 * @file UTHorizontalBarChart.qml
 * @brief 水平条形图组件 — 左标签 + 条形 + 右数值，底部刻度轴
 */

// ========================================================================
// ========================================================================

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

Rectangle {
    id: root

    // ── Theme ────────────────────────────────────────────────────
    readonly property bool isDark: UTTheme.isDark !== undefined && UTTheme.isDark

    // ── 数据 ──────────────────────────────────────────────────────
    /// 数据点数组: [{label, value, color?}]
    /// color 可选，不提供时使用 barColor
    property var dataPoints: [
        {
            label: "电影名",
            value: 1000000
        },
        {
            label: "电影名",
            value: 950000
        },
        {
            label: "电影名",
            value: 900000
        },
        {
            label: "电影名",
            value: 891255
        }
    ]

    // ── 样式 ──────────────────────────────────────────────────────
    /// 默认条形颜色
    property color barColor: UTTheme.primaryColor
    /// 单条高度 (含间距)
    property real barHeight: 28
    /// 行间距
    property real rowSpacing: 6
    /// 条形圆角
    property real barRadius: 4
    /// 左侧标签区域宽度
    property real labelWidth: 80
    /// 右侧数值区域宽度
    property real valueWidth: 80
    /// 条形区域与标签/数值的间距
    property real barMargin: 8
    /// 数值跟随条形 (false=右侧固定, true=紧随条形末端)
    property bool valueFollowBar: true
    /// 圆角
    property real cardRadius: UTTheme.radius || 8

    // ── 刻度 ──────────────────────────────────────────────────────
    /// 是否显示底部刻度
    property bool showScale: true
    /// 刻度数量 (含 0)
    property int scaleTicks: 6
    /// 刻度区域高度
    property real scaleHeight: 24

    // ── 动画 ──────────────────────────────────────────────────────
    /// 条形增长动画时长 (ms)
    property int barAnimationDuration: 600

    // ── Hover 提示状态 ──────────────────────────────────────────
    property int _hoveredIndex: -1
    property string _hoveredLabel: ""
    property string _hoveredValue: ""
    property point _hoveredPos: Qt.point(0, 0)

    // ── 无障碍 ──────────────────────────────────────────────────
    property string accName: "Horizontal Bar Chart"
    property int accRole: Accessible.StaticText
    Accessible.name: accName
    Accessible.role: accRole

    // ── 计算属性 ──────────────────────────────────────────────────
    /// 数据中的最大值 (自动计算)
    property real maxValue: {
        var max = 0;
        for (var i = 0; i < root.dataPoints.length; i++) {
            const value = Number((root.dataPoints[i] || {}).value || 0);
            if (value > max)
                max = value;
        }
        return max > 0 ? max : 100;
    }

    /// 格式化刻度值 (如 1000000 → "1M")
    property var scaleFormatter: function (val) {
        if (val >= 1000000)
            return (val / 1000000).toFixed(1).replace(/\.0$/, '') + "M";
        if (val >= 1000)
            return (val / 1000).toFixed(1).replace(/\.0$/, '') + "k";
        return String(Math.round(val));
    }

    /// 格式化数值显示
    property var valueFormatter: function (val) {
        return val.toLocaleString(Qt.locale(), 'f', 0);
    }

    TextMetrics {
        id: followValueMetrics
        font.family: UTTheme.fontFamily
        font.pixelSize: 13
        font.weight: Font.DemiBold
        text: root.valueFormatter(root.maxValue)
    }

    /// 跟随模式按最大值文本预留空间，并由 valueWidth 限制最大占用
    readonly property real _followValueReservedWidth: Math.min(valueWidth, Math.ceil(followValueMetrics.advanceWidth) + 2)

    /// 彩色条形可用宽度；跟随模式为轨道内的数值预留空间
    property real barAreaWidth: Math.max(0, width - contentPadding * 2 - labelWidth - barMargin - (valueFollowBar ? _followValueReservedWidth : valueWidth) - barMargin)

    /// 内容区上下边距
    readonly property real contentPadding: 12

    /// 条形区域总高度
    readonly property real barsTotalHeight: dataPoints.length * barHeight + Math.max(0, dataPoints.length - 1) * rowSpacing
    readonly property real _scaleReservedHeight: showScale ? scaleHeight + 8 : 0
    readonly property real _availablePadding: Math.max(0, (height - barsTotalHeight - _scaleReservedHeight) / 2)
    readonly property real _effectiveContentPadding: Math.min(contentPadding, _availablePadding)

    implicitWidth: 400
    implicitHeight: contentPadding * 2 + barsTotalHeight + _scaleReservedHeight
    color: "transparent"

    // ── 背景 ────────────────────────────────────────────────────
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.cardRadius
        color: UTTheme.surfaceColor
        border.color: Qt.rgba(UTTheme.borderColor.r || 0.84, UTTheme.borderColor.g || 0.86, UTTheme.borderColor.b || 0.89, 0.4)
        border.width: 1

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: UTTheme.shadowColor
            shadowBlur: 1.0
            shadowHorizontalOffset: 2
            shadowVerticalOffset: 2
        }
    }

    // ── 条形区域 ────────────────────────────────────────────────
    Column {
        id: barsColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: root._effectiveContentPadding
        anchors.bottom: scaleArea.top
        anchors.bottomMargin: root.showScale ? 8 : root._effectiveContentPadding
        spacing: root.rowSpacing
        clip: true

        Repeater {
            model: root.dataPoints
            delegate: barDelegate
        }
    }

    // ── 单行条形代理 ────────────────────────────────────────────
    Component {
        id: barDelegate

        Item {
            id: barRow
            required property int index
            required property var modelData
            readonly property var dataPoint: modelData || ({})

            width: barsColumn.width
            height: root.barHeight

            property bool hovered: false

            // 浮动缩放
            transform: Scale {
                origin.x: barRow.width / 2
                origin.y: barRow.height / 2
                xScale: 1 + (barRow.hovered ? 0.03 : 0)
                yScale: 1 + (barRow.hovered ? 0.08 : 0)

                Behavior on xScale {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on yScale {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                }
            }

            // 浮动阴影
            Rectangle {
                anchors.fill: barTrack
                anchors.margins: barRow.hovered ? 4 : 0
                radius: root.barRadius + 2
                color: "transparent"

                Rectangle {
                    anchors.fill: parent
                    radius: root.barRadius
                    color: Qt.rgba(0, 0, 0, barRow.hovered ? 0.12 : 0)
                    visible: barRow.hovered

                    Behavior on color {
                        ColorAnimation {
                            duration: 200
                        }
                    }
                }
            }

            // Hover 检测
            MouseArea {
                id: barHover
                anchors.fill: parent
                hoverEnabled: true
                preventStealing: true
                onEntered: {
                    barRow.hovered = true;
                    root._hoveredIndex = barRow.index;
                    root._hoveredLabel = barRow.dataPoint.label || "";
                    root._hoveredValue = root.valueFormatter(barRow.dataPoint.value || 0);
                }
                onExited: {
                    barRow.hovered = false;
                    root._hoveredIndex = -1;
                }
                onPositionChanged: function (mouse) {
                    var pos = barHover.mapToItem(root, mouse.x, mouse.y);
                    root._hoveredPos = pos;
                    var tx = pos.x + 12;
                    var ty = pos.y - tooltipRoot.height - 8;
                    if (ty < root.contentPadding)
                        ty = pos.y + 8;
                    if (tx + tooltipRoot.width > root.width)
                        tx = pos.x - tooltipRoot.width - 12;
                    tooltipRoot.x = tx;
                    tooltipRoot.y = ty;
                }
            }

            // 左侧标签
            Text {
                anchors.left: parent.left
                anchors.leftMargin: root.contentPadding
                anchors.verticalCenter: parent.verticalCenter
                width: root.labelWidth - 4
                text: barRow.dataPoint.label || ""
                color: barRow.hovered ? (UTTheme.primaryColor) : (UTTheme.primaryTextColor)
                font.pixelSize: 13
                font.weight: barRow.hovered ? Font.DemiBold : Font.Normal
                font.family: UTTheme.fontFamily
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight

                Behavior on color {
                    ColorAnimation {
                        duration: 180
                    }
                }
            }

            // 条形背景轨道
            Rectangle {
                id: barTrack
                anchors.left: parent.left
                anchors.leftMargin: root.contentPadding + root.labelWidth + root.barMargin
                anchors.verticalCenter: parent.verticalCenter
                width: root.barAreaWidth + (root.valueFollowBar ? root._followValueReservedWidth + root.barMargin : 0)
                height: root.barHeight - 4
                radius: root.barRadius
                color: root.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)

                // 条形填充
                Rectangle {
                    id: barFill
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    height: parent.height
                    radius: root.barRadius
                    color: barRow.dataPoint.color !== undefined ? barRow.dataPoint.color : root.barColor
                    width: {
                        var ratio = (barRow.dataPoint.value || 0) / root.maxValue;
                        return ratio * root.barAreaWidth;
                    }

                    Behavior on width {
                        enabled: root.barAnimationDuration > 0
                        NumberAnimation {
                            duration: root.barAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            // 数值（跟随条形）
            Text {
                id: followValueText
                visible: root.valueFollowBar
                x: barTrack.x + barFill.width + root.barMargin
                width: Math.max(0, Math.min(root._followValueReservedWidth, barRow.width - root.contentPadding - x))
                anchors.verticalCenter: parent.verticalCenter
                text: root.valueFormatter(barRow.dataPoint.value || 0)
                color: barRow.hovered ? (UTTheme.primaryColor) : (UTTheme.primaryTextColor)
                font.pixelSize: 13
                font.weight: barRow.hovered ? Font.DemiBold : Font.Medium
                font.family: UTTheme.fontFamily
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
                z: 2

                Behavior on color {
                    ColorAnimation {
                        duration: 180
                    }
                }
            }

            // 数值（右侧固定）
            Text {
                anchors.right: parent.right
                anchors.rightMargin: root.contentPadding + 4
                anchors.verticalCenter: parent.verticalCenter
                width: root.valueWidth - 8
                visible: !root.valueFollowBar
                text: root.valueFormatter(barRow.dataPoint.value || 0)
                color: barRow.hovered ? (UTTheme.primaryColor) : (UTTheme.primaryTextColor)
                font.pixelSize: 13
                font.weight: barRow.hovered ? Font.DemiBold : Font.Medium
                font.family: UTTheme.fontFamily
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight

                Behavior on color {
                    ColorAnimation {
                        duration: 180
                    }
                }
            }
        }
    }

    // ── 底部刻度代理 ────────────────────────────────────────────
    Component {
        id: tickDelegate

        Item {
            id: tickItem
            required property int index

            x: tickItem.index === 0 ? 0 : (parent.width * tickItem.index / (root.scaleTicks - 1)) - width / 2
            y: 0
            width: 40
            height: parent.height

            // 刻度线
            Rectangle {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                width: 1
                height: 6
                color: UTTheme.borderColor
            }

            // 刻度值
            Text {
                anchors.top: parent.top
                anchors.topMargin: 8
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    var val = root.maxValue * tickItem.index / (root.scaleTicks - 1);
                    return root.scaleFormatter(val);
                }
                color: UTTheme.secondaryTextColor
                font.pixelSize: 11
                font.family: UTTheme.fontFamily
            }
        }
    }

    // ── 底部刻度轴 ──────────────────────────────────────────────
    Item {
        id: scaleArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.showScale ? 4 : 0
        height: root.showScale ? root.scaleHeight : 0
        visible: root.showScale

        // 刻度容器 (与条形区域对齐)
        Item {
            anchors.left: parent.left
            anchors.leftMargin: root.contentPadding + root.labelWidth + root.barMargin
            anchors.bottom: parent.bottom
            width: root.barAreaWidth
            height: parent.height

            Repeater {
                model: root.scaleTicks
                delegate: tickDelegate
            }
        }
    }

    // ── Hover 提示 (根层级，避免裁剪) ────────────────────────────
    Rectangle {
        id: tooltipRoot
        visible: root._hoveredIndex >= 0
        width: tooltipRow.implicitWidth + 20
        height: tooltipRow.implicitHeight + 16
        radius: 8
        color: UTTheme.backgroundColor
        border.color: UTTheme.focusColor
        border.width: 1
        z: 10

        Row {
            id: tooltipRow
            x: 10
            y: 8
            spacing: 6

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root._hoveredIndex >= 0 && root._hoveredIndex < root.dataPoints.length && root.dataPoints[root._hoveredIndex].color ? root.dataPoints[root._hoveredIndex].color : root.barColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: root._hoveredLabel
                color: UTTheme.primaryTextColor
                font.pixelSize: 13
                font.bold: true
                font.family: UTTheme.fontFamily
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: root._hoveredValue
                color: UTTheme.primaryTextColor
                font.pixelSize: 13
                font.family: UTTheme.fontFamily
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
