/*
 * @file UTHsvColorWheel.qml
 * @brief Theme-aware HSV range visualizer and editor.
 */

import QtQuick

Item {
    id: root

    property real hueMin: 0
    property real hueMax: 180
    property real saturationMin: 0
    property real saturationMax: 255
    property real valueMin: 0
    property real valueMax: 255

    property real hueRange: 180
    property real saturationRange: 255
    property real valueRange: 255
    property color backgroundColor: UTTheme.isDark ? "#333333" : "#d8d8d8"
    property color boundaryColor: UTTheme.primaryTextColor

    signal hsvChanged(real hueMinimum, real hueMaximum, real saturationMinimum, real saturationMaximum, real valueMinimum, real valueMaximum)

    implicitWidth: 180
    implicitHeight: 180

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, Number(value)));
    }

    function normalizedHue(value) {
        return clamp(value, 0, root.hueRange) / Math.max(root.hueRange, 1);
    }

    function hueInRange(value) {
        const low = root.clamp(root.hueMin, 0, root.hueRange);
        const high = root.clamp(root.hueMax, 0, root.hueRange);
        return value >= low && value <= high;
    }

    function averageSaturation() {
        return clamp((saturationMin + saturationMax) / 2, 0, saturationRange);
    }

    function averageValue() {
        return clamp((valueMin + valueMax) / 2, 0, valueRange);
    }

    function setHsvRange(nextHueMin, nextHueMax, nextSaturationMin, nextSaturationMax, nextValueMin, nextValueMax) {
        root.hueMin = root.clamp(nextHueMin, 0, root.hueRange);
        root.hueMax = root.clamp(nextHueMax, root.hueMin, root.hueRange);
        root.saturationMin = root.clamp(nextSaturationMin, 0, root.saturationRange);
        root.saturationMax = root.clamp(nextSaturationMax, root.saturationMin, root.saturationRange);
        root.valueMin = root.clamp(nextValueMin, 0, root.valueRange);
        root.valueMax = root.clamp(nextValueMax, root.valueMin, root.valueRange);
        root.hsvChanged(root.hueMin, root.hueMax, root.saturationMin, root.saturationMax, root.valueMin, root.valueMax);
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const context = getContext("2d");
            const centerX = width / 2;
            const centerY = height / 2;
            const radius = Math.max(1, Math.min(width, height) / 2 - 10);
            const startAngle = -Math.PI / 2;
            const averageSaturation = root.averageSaturation() / Math.max(root.saturationRange, 1);
            const averageValue = root.averageValue() / Math.max(root.valueRange, 1);

            context.clearRect(0, 0, width, height);
            context.beginPath();
            context.arc(centerX, centerY, radius, 0, Math.PI * 2);
            context.fillStyle = root.backgroundColor;
            context.fill();

            // Use sub-degree slices to approximate Python's continuous
            // conical gradient without visible seams between sectors.
            const segments = 720;
            for (let index = 0; index < segments; ++index) {
                const hue = index / segments * root.hueRange;
                const angle = startAngle + index / segments * Math.PI * 2;
                const nextAngle = startAngle + (index + 1) / segments * Math.PI * 2;
                context.beginPath();
                context.moveTo(centerX, centerY);
                context.arc(centerX, centerY, radius, angle, nextAngle);
                context.closePath();
                context.fillStyle = root.hueInRange(hue) ? Qt.hsva(root.normalizedHue(hue), averageSaturation, averageValue, 1) : root.backgroundColor;
                context.fill();
            }

            context.beginPath();
            context.moveTo(centerX, centerY);
            context.lineTo(centerX + Math.cos(startAngle + root.normalizedHue(root.hueMin) * Math.PI * 2) * radius, centerY + Math.sin(startAngle + root.normalizedHue(root.hueMin) * Math.PI * 2) * radius);
            context.moveTo(centerX, centerY);
            context.lineTo(centerX + Math.cos(startAngle + root.normalizedHue(root.hueMax) * Math.PI * 2) * radius, centerY + Math.sin(startAngle + root.normalizedHue(root.hueMax) * Math.PI * 2) * radius);
            context.strokeStyle = root.boundaryColor;
            context.lineWidth = 2;
            context.stroke();
        }

        Connections {
            target: root
            function onHueMinChanged() {
                canvas.requestPaint();
            }
            function onHueMaxChanged() {
                canvas.requestPaint();
            }
            function onSaturationMinChanged() {
                canvas.requestPaint();
            }
            function onSaturationMaxChanged() {
                canvas.requestPaint();
            }
            function onValueMinChanged() {
                canvas.requestPaint();
            }
            function onValueMaxChanged() {
                canvas.requestPaint();
            }
            function onWidthChanged() {
                canvas.requestPaint();
            }
            function onHeightChanged() {
                canvas.requestPaint();
            }
        }
    }
}
