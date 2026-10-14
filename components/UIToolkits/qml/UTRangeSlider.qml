/*
 * @file UTRangeSlider.qml
 * @brief 双滑块区间选择器 — 基于 Qt RangeSlider，支持左右两端数值显示
 */

// ========================================================================
// ========================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

RangeSlider {
    id: root

    // ── 便捷别名 ─────────────────────────────────────────────────────
    /// 左滑块当前值（区间下限），等价于 first.value
    property alias minValue: root.first.value
    /// 右滑块当前值（区间上限），等价于 second.value
    property alias maxValue: root.second.value

    // ── 数值显示 ─────────────────────────────────────────────────────
    property bool showValue: true
    property string valuePrefix: ""
    property string valueSuffix: ""
    property int valuePrecision: -1  // -1 自动，>=0 固定小数位
    property int sideEditorWidth: 60
    property bool keyboardTracking: false
    enum ValueDisplayMode {
        AboveHandles,
        SideValues
    }
    /// 数值显示位置：默认跟随滑块上方；SideValues 固定显示在轨道左右两侧。
    property int valueDisplayMode: UTRangeSlider.AboveHandles
    readonly property bool _showsAboveValues: showValue && valueDisplayMode === UTRangeSlider.AboveHandles
    readonly property bool _showsSideValues: showValue && valueDisplayMode === UTRangeSlider.SideValues
    readonly property bool _usesIntegerEditors: stepSize >= 1 && stepSize % 1 === 0 && from % 1 === 0 && to % 1 === 0
    property bool _sideEditorsReady: false
    property bool _syncingSideEditors: false
    property bool _rangeChangePending: false
    /// 两端之间的最小距离；默认一个步长，设为 0 可选择相等的上下限。
    property real minimumSeparation: Math.max(Number(stepSize), 0.000001)

    // ── 信号 ─────────────────────────────────────────────────────────
    signal rangeChanged(real min, real max)

    // ── 主题 ─────────────────────────────────────────────────────────

    property color activeColor: UTTheme.primaryColor
    property color inactiveColor: UTTheme.isDark ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
    property color thumbColor: "#ffffff"
    property color valueTextColor: UTTheme.secondaryTextColor

    // ── 无障碍 ───────────────────────────────────────────────────────
    property string accName: ""
    Accessible.name: accName !== "" ? accName : "RangeSlider " + Math.round(first.value) + " to " + Math.round(second.value)
    Accessible.role: Accessible.Slider

    // ── 尺寸 ─────────────────────────────────────────────────────────
    implicitWidth: _showsSideValues ? 300 : 200
    implicitHeight: _showsAboveValues ? 54 : 30
    leftPadding: _showsSideValues ? sideEditorWidth + 10 : 12
    rightPadding: _showsSideValues ? sideEditorWidth + 10 : 12
    topPadding: 12
    bottomPadding: 12

    // ── 连接信号 ─────────────────────────────────────────────────────
    Connections {
        target: root.first
        function onValueChanged() {
            root._enforceMinimumSeparation(true);
            root._syncSideEditors();
            root._scheduleRangeChanged();
        }
    }
    Connections {
        target: root.second
        function onValueChanged() {
            root._enforceMinimumSeparation(false);
            root._syncSideEditors();
            root._scheduleRangeChanged();
        }
    }

    // ── 数值格式化 ───────────────────────────────────────────────────
    function _formatValue(v) {
        if (valuePrecision >= 0)
            return valuePrefix + Number(v).toFixed(valuePrecision) + valueSuffix;
        return valuePrefix + Number(v).toFixed(stepSize >= 1 && stepSize % 1 === 0 ? 0 : 1) + valueSuffix;
    }

    function _scheduleRangeChanged() {
        if (root._rangeChangePending)
            return;
        root._rangeChangePending = true;
        Qt.callLater(function() {
            root._rangeChangePending = false;
            root.rangeChanged(root.first.value, root.second.value);
        });
    }

    // ── 左滑块 ───────────────────────────────────────────────────────
    first.handle: Rectangle {
        id: firstHandleItem
        objectName: "firstHandleItem"
        x: root.leftPadding + root.first.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: root.first.hovered ? 24 : 20
        implicitHeight: root.first.hovered ? 24 : 20
        // 滑块始终保持圆形；悬停时仅放大与加粗边框，避免变成圆角方块。
        radius: width / 2
        color: root.enabled ? root.thumbColor : "#dddddd"
        border.width: root.first.hovered ? 3 : 2
        border.color: root.activeColor

        Behavior on implicitWidth {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
        Behavior on implicitHeight {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
        Behavior on border.width {
            NumberAnimation {
                duration: 120
            }
        }
        Behavior on x {
            NumberAnimation {
                duration: 120
            }
        }

        // 数值标签
        Label {
            id: minValueLabel
            visible: root._showsAboveValues
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: 4
            text: root._formatValue(root.first.value)
            color: root.valueTextColor
            font.pixelSize: 11
        }
    }

    // ── 右滑块 ───────────────────────────────────────────────────────
    second.handle: Rectangle {
        id: secondHandleItem
        objectName: "secondHandleItem"
        x: root.leftPadding + root.second.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: root.second.hovered ? 24 : 20
        implicitHeight: root.second.hovered ? 24 : 20
        // 滑块始终保持圆形；悬停时仅放大与加粗边框，避免变成圆角方块。
        radius: width / 2
        color: root.enabled ? root.thumbColor : "#dddddd"
        border.width: root.second.hovered ? 3 : 2
        border.color: root.activeColor

        Behavior on implicitWidth {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
        Behavior on implicitHeight {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
        Behavior on border.width {
            NumberAnimation {
                duration: 120
            }
        }
        Behavior on x {
            NumberAnimation {
                duration: 120
            }
        }

        // 数值标签
        Label {
            id: maxValueLabel
            visible: root._showsAboveValues
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: 4
            text: root._formatValue(root.second.value)
            color: root.valueTextColor
            font.pixelSize: 11
        }
    }

    // ── 背景轨道 ─────────────────────────────────────────────────────
    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: 200
        implicitHeight: 6
        width: root.availableWidth
        height: 6
        radius: 3
        color: root.inactiveColor

        // 激活区间（两滑块之间的填充）
        Rectangle {
            x: root.first.visualPosition * parent.width
            width: (root.second.visualPosition - root.first.visualPosition) * parent.width
            height: parent.height
            radius: parent.radius
            color: root.activeColor

            Behavior on x {
                NumberAnimation {
                    duration: 120
                }
            }
            Behavior on width {
                NumberAnimation {
                    duration: 120
                }
            }
        }
    }

    function _enforceMinimumSeparation(firstChanged) {
        const gap = Math.min(Math.max(minimumSeparation, 0), Math.max(0, to - from));
        if (second.value - first.value >= gap)
            return;
        if (firstChanged)
            root.setValues(Math.max(from, second.value - gap), second.value);
        else
            root.setValues(first.value, Math.min(to, first.value + gap));
    }

    function _setFirstValue(value) {
        const next = Math.min(value, second.value - minimumSeparation);
        root.setValues(next, second.value);
    }

    function _setSecondValue(value) {
        const next = Math.max(value, first.value + minimumSeparation);
        root.setValues(first.value, next);
    }

    function _syncSideEditors() {
        if (!root._sideEditorsReady)
            return;
        root._syncingSideEditors = true;
        if (!sideLeftInteger.activeFocus)
            sideLeftInteger.value = Math.round(first.value);
        if (!sideRightInteger.activeFocus)
            sideRightInteger.value = Math.round(second.value);
        if (!sideLeftDecimal.activeFocus)
            sideLeftDecimal.value = first.value;
        if (!sideRightDecimal.activeFocus)
            sideRightDecimal.value = second.value;
        root._syncingSideEditors = false;
    }

    Component.onCompleted: {
        root._sideEditorsReady = true;
        root._syncSideEditors();
    }

    UTSpinBox {
        id: sideLeftInteger
        objectName: "sideLeftInteger"
        keyboardTracking: root.keyboardTracking
        z: 10
        visible: root._showsSideValues && root._usesIntegerEditors
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: root.sideEditorWidth
        height: 32
        from: Math.ceil(root.from)
        to: Math.floor(root.second.value - root.minimumSeparation)
        stepSize: Math.max(1, Math.round(root.stepSize))
        editable: true
        activeFocusOnTab: true
        onValueChanged: {
            if (root._sideEditorsReady && !root._syncingSideEditors)
                root._setFirstValue(value);
        }
    }

    UTDoubleSpinBox {
        id: sideRightDecimal
        objectName: "sideRightDecimal"
        keyboardTracking: root.keyboardTracking
        z: 10
        visible: root._showsSideValues && !root._usesIntegerEditors
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: root.sideEditorWidth
        height: 32
        from: root.first.value + root.minimumSeparation
        to: root.to
        stepSize: root.stepSize
        decimals: root.valuePrecision >= 0 ? root.valuePrecision : 2
        activeFocusOnTab: true
        onValueChanged: {
            if (root._sideEditorsReady && !root._syncingSideEditors)
                root._setSecondValue(value);
        }
    }

    UTDoubleSpinBox {
        id: sideLeftDecimal
        objectName: "sideLeftDecimal"
        keyboardTracking: root.keyboardTracking
        z: 10
        visible: root._showsSideValues && !root._usesIntegerEditors
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: root.sideEditorWidth
        height: 32
        from: root.from
        to: root.second.value - root.minimumSeparation
        stepSize: root.stepSize
        decimals: root.valuePrecision >= 0 ? root.valuePrecision : 2
        activeFocusOnTab: true
        onValueChanged: {
            if (root._sideEditorsReady && !root._syncingSideEditors)
                root._setFirstValue(value);
        }
    }

    UTSpinBox {
        id: sideRightInteger
        objectName: "sideRightInteger"
        keyboardTracking: root.keyboardTracking
        z: 10
        visible: root._showsSideValues && root._usesIntegerEditors
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: root.sideEditorWidth
        height: 32
        from: Math.ceil(root.first.value + root.minimumSeparation)
        to: Math.floor(root.to)
        stepSize: Math.max(1, Math.round(root.stepSize))
        editable: true
        activeFocusOnTab: true
        onValueChanged: {
            if (root._sideEditorsReady && !root._syncingSideEditors)
                root._setSecondValue(value);
        }
    }
}
