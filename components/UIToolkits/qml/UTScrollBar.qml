/*
 * @file UTScrollBar.qml
 * @brief UIToolkits 的 UTScrollBar.qml 组件。
 */

import QtQuick

Item {
    id: root

    clip: true

    property Flickable target: null

    readonly property color trackColor: UTTheme.scrollbarTrackColor
    readonly property color thumbNormalColor: UTTheme.scrollbarThumbNormalColor
    readonly property color thumbHoverColor: UTTheme.scrollbarThumbHoverColor
    readonly property color thumbPressedColor: UTTheme.scrollbarThumbPressedColor

    property real thickness: 12
    implicitWidth: thickness

    property bool _hovered: false
    property bool _dragging: false

    readonly property real _viewH: target ? target.height : 0
    readonly property real _contH: target ? target.contentHeight : 0
    readonly property real _scrY: target ? target.contentY : 0
    readonly property real _ratio: _contH > 0 ? _viewH / _contH : 1.0
    readonly property real _thumbH: Math.max(thickness * 1.5, _ratio * height)
    readonly property real _maxY: height - _thumbH
    readonly property real _thumbY: _contH > _viewH ? (_scrY / (_contH - _viewH)) * _maxY : 0

    visible: target && _contH > _viewH

    Rectangle {
        id: track
        anchors.fill: parent
        color: root.trackColor
        radius: root.thickness / 2
    }

    Rectangle {
        id: thumb
        objectName: "thumb"
        x: (root.width - width) / 2
        y: root._thumbY
        width: Math.min(root.thickness, 8)
        height: root._thumbH
        radius: width / 2

        color: {
            if (root._dragging)
                return root.thumbPressedColor;
            if (root._hovered)
                return root.thumbHoverColor;
            return root.thumbNormalColor;
        }

        Behavior on color {
            ColorAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }

        MouseArea {
            id: thumbMouseArea
            objectName: "thumbMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onEntered: root._hovered = true
            onExited: root._hovered = false
            onPressed: root._dragging = true
            onReleased: {
                root._dragging = false;
                thumb.y = Qt.binding(function () {
                    return root._thumbY;
                });
            }
            onCanceled: {
                root._dragging = false;
                thumb.y = Qt.binding(function () {
                    return root._thumbY;
                });
            }

            drag {
                target: thumb
                axis: Drag.YAxis
                minimumY: 0
                maximumY: root._maxY
            }
        }

        onYChanged: {
            if (root._dragging && root.target && root._maxY > 0)
                root.target.contentY = (y / root._maxY) * (root._contH - root._viewH);
        }
    }
}
