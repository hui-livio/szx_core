/*
 * @file UTTooltip.qml
 * @brief UIToolkits 的 UTTooltip.qml 组件。
 */

pragma Singleton

import QtQuick
import QtQuick.Controls.Basic

QtObject {
    id: root

    enum Placement {
        Auto,
        Below,
        Above,
        FollowMouse
    }

    property int delay: 600
    property int timeout: -1
    property int defaultPlacement: UTTooltip.Auto
    property real gap: 8
    property real screenMargin: 8
    property real maximumWidth: 360
    property point mouseOffset: Qt.point(12, 16)
    readonly property color backgroundColor: UTTheme.surfaceColor
    readonly property color textColor: UTTheme.primaryTextColor
    readonly property color borderColor: UTTheme.borderColor
    readonly property real radius: Math.max(4, UTTheme.radius * 0.5)
    readonly property color renderedBackgroundColor: tooltipBackground.color
    readonly property color renderedTextColor: tooltipText.color
    readonly property color renderedBorderColor: tooltipBackground.border.color
    readonly property real renderedRadius: tooltipBackground.radius
    readonly property string renderedFontFamily: tooltipText.font.family

    property Item currentTarget: null
    property int currentPlacement: defaultPlacement
    property point currentPointerPosition: Qt.point(-1, -1)
    property int currentTimeout: timeout
    property string currentText: ""

    property Item popup: Item {
        id: sharedPopup

        parent: root.currentTarget ? root.currentTarget.Overlay.overlay : null
        enabled: false
        visible: false
        z: 1000000

        readonly property real leftPadding: 10
        readonly property real rightPadding: 10
        readonly property real topPadding: 6
        readonly property real bottomPadding: 6
        readonly property real availableWidth: Math.max(
                                                   0, width - leftPadding - rightPadding)
        readonly property alias background: tooltipBackground
        readonly property alias contentItem: tooltipText

        implicitWidth: Math.min(root.maximumWidth,
                                tooltipText.implicitWidth + leftPadding + rightPadding)
        implicitHeight: tooltipText.implicitHeight + topPadding + bottomPadding
        width: implicitWidth
        height: implicitHeight
        onWidthChanged: root.scheduleReposition()
        onHeightChanged: root.scheduleReposition()

        Rectangle {
            id: tooltipBackground

            anchors.fill: parent
            radius: root.radius
            color: root.backgroundColor
            border.width: 1
            border.color: root.borderColor
        }

        Text {
            id: tooltipText

            x: sharedPopup.leftPadding
            y: sharedPopup.topPadding
            width: sharedPopup.availableWidth
            height: sharedPopup.height - sharedPopup.topPadding - sharedPopup.bottomPadding
            text: root.currentText
            color: root.textColor
            font.family: UTTheme.fontFamily
            font.pixelSize: 13
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
        }
    }

    property Timer showTimer: Timer {
        interval: Math.max(0, root.delay)
        repeat: false
        onTriggered: root.openPopup()
    }

    property Timer timeoutTimer: Timer {
        interval: Math.max(1, root.currentTimeout)
        repeat: false
        onTriggered: root.hide(root.currentTarget)
    }

    property Timer trackingTimer: Timer {
        interval: 16
        repeat: true
        running: root.currentTarget !== null
        onTriggered: root.reposition()
    }

    property Connections targetConnections: Connections {
        target: root.currentTarget

        function onXChanged() {
            root.scheduleReposition();
        }

        function onYChanged() {
            root.scheduleReposition();
        }

        function onWidthChanged() {
            root.scheduleReposition();
        }

        function onHeightChanged() {
            root.scheduleReposition();
        }

        function onVisibleChanged() {
            if (!root.isTargetDisplayable(root.currentTarget))
                root.hide(root.currentTarget);
        }

        function onEnabledChanged() {
            if (!root.isTargetDisplayable(root.currentTarget))
                root.hide(root.currentTarget);
        }
    }

    property Connections windowConnections: Connections {
        target: root.currentTarget ? root.currentTarget.Window.window : null

        function onWidthChanged() {
            root.scheduleReposition();
        }

        function onHeightChanged() {
            root.scheduleReposition();
        }

        function onVisibleChanged() {
            if (!root.currentTarget
                    || !root.currentTarget.Window.window
                    || !root.currentTarget.Window.window.visible)
                root.hide(root.currentTarget);
        }
    }

    function update(target, text, visible, requestedTimeout, placement, pointerPosition) {
        if (!target)
            return;

        const shouldShow = Boolean(visible && text);
        if (!shouldShow) {
            if (root.currentTarget === target)
                root.hide(target);
            return;
        }

        const alreadyTracking = root.currentTarget === target;
        if (root.currentTarget && !alreadyTracking)
            root.hide(root.currentTarget);

        root.currentTarget = target;
        root.currentPlacement = placement === undefined ? root.defaultPlacement : placement;
        root.currentPointerPosition = pointerPosition === undefined
                                    ? Qt.point(-1, -1) : pointerPosition;
        root.currentTimeout = requestedTimeout === undefined ? root.timeout : requestedTimeout;
        root.currentText = text;

        if (alreadyTracking) {
            root.scheduleReposition();
            return;
        }

        if (root.delay > 0)
            root.showTimer.restart();
        else
            root.openPopup();
    }

    function updatePointer(target, placement, pointerPosition) {
        if (!target || root.currentTarget !== target)
            return;

        root.currentPlacement = placement === undefined ? root.defaultPlacement : placement;
        root.currentPointerPosition = pointerPosition === undefined
                                    ? Qt.point(-1, -1) : pointerPosition;
        root.scheduleReposition();
    }

    function openPopup() {
        const target = root.currentTarget;
        if (!target || !root.isTargetDisplayable(target)) {
            root.hide(target);
            return;
        }

        root.popup.visible = true;
        if (root.currentTimeout > 0)
            root.timeoutTimer.restart();
        root.scheduleReposition();
    }

    function scheduleReposition() {
        if (root.currentTarget)
            Qt.callLater(root.reposition);
    }

    function hasValidPointer(pointerPosition) {
        return pointerPosition
            && isFinite(pointerPosition.x)
            && isFinite(pointerPosition.y)
            && pointerPosition.x >= 0
            && pointerPosition.y >= 0;
    }

    function resolvedPosition(boundsSize, targetRect, tooltipSize, placement, pointerPosition) {
        const margin = root.screenMargin;
        let x = targetRect.x + (targetRect.width - tooltipSize.width) / 2;
        let y = targetRect.y + targetRect.height + root.gap;

        const hasPointer = root.hasValidPointer(pointerPosition);
        const followPointer = placement === UTTooltip.FollowMouse
                           || (placement === UTTooltip.Auto && hasPointer);

        if (followPointer) {
            const pointer = hasPointer
                          ? pointerPosition
                          : Qt.point(targetRect.x + targetRect.width / 2,
                                     targetRect.y + targetRect.height / 2);
            x = pointer.x + root.mouseOffset.x;
            y = pointer.y + root.mouseOffset.y;
            if (x + tooltipSize.width > boundsSize.width - margin)
                x = pointer.x - tooltipSize.width - root.mouseOffset.x;
            if (y + tooltipSize.height > boundsSize.height - margin)
                y = pointer.y - tooltipSize.height - root.mouseOffset.y;
        } else {
            const preferAbove = placement === UTTooltip.Above;
            const belowY = targetRect.y + targetRect.height + root.gap;
            const aboveY = targetRect.y - tooltipSize.height - root.gap;
            y = preferAbove ? aboveY : belowY;

            if (preferAbove && y < margin)
                y = belowY;
            else if (!preferAbove && y + tooltipSize.height > boundsSize.height - margin)
                y = aboveY;
        }

        x = Math.max(margin, Math.min(x, boundsSize.width - tooltipSize.width - margin));
        y = Math.max(margin, Math.min(y, boundsSize.height - tooltipSize.height - margin));
        return Qt.point(x, y);
    }

    function isTargetDisplayable(target, bounds) {
        if (!target || !target.visible || !target.enabled)
            return false;

        const window = target.Window.window;
        if (!window || !window.visible)
            return false;

        const targetBounds = bounds || window.contentItem;
        if (!targetBounds)
            return false;

        let visibleRect = target.mapToItem(targetBounds, Qt.rect(0, 0, target.width, target.height));
        let ancestor = target.parent;
        while (ancestor) {
            if (ancestor.visible !== undefined && !ancestor.visible)
                return false;
            if (ancestor.opacity !== undefined && ancestor.opacity <= 0)
                return false;
            if (ancestor.clip === true) {
                const clipRect = ancestor.mapToItem(
                                   targetBounds, Qt.rect(0, 0, ancestor.width, ancestor.height));
                visibleRect = root.intersectedRect(visibleRect, clipRect);
                if (visibleRect.width <= 0 || visibleRect.height <= 0)
                    return false;
            }
            ancestor = ancestor.parent;
        }

        visibleRect = root.intersectedRect(
                    visibleRect, Qt.rect(0, 0, targetBounds.width, targetBounds.height));
        return visibleRect.width > 0 && visibleRect.height > 0;
    }

    function intersectedRect(first, second) {
        const left = Math.max(first.x, second.x);
        const top = Math.max(first.y, second.y);
        const right = Math.min(first.x + first.width, second.x + second.width);
        const bottom = Math.min(first.y + first.height, second.y + second.height);
        return Qt.rect(left, top, Math.max(0, right - left), Math.max(0, bottom - top));
    }

    function reposition() {
        const target = root.currentTarget;
        const tooltip = root.popup;
        if (!target || !tooltip.parent)
            return;

        const window = target.Window.window;
        const bounds = window ? window.contentItem : tooltip.parent;
        if (!bounds)
            return;
        if (!root.isTargetDisplayable(target, bounds)) {
            root.hide(target);
            return;
        }

        const targetRect = target.mapToItem(bounds, Qt.rect(0, 0, target.width, target.height));
        const tooltipWidth = Math.max(1, tooltip.width || tooltip.implicitWidth);
        const tooltipHeight = Math.max(1, tooltip.height || tooltip.implicitHeight);
        const pointer = root.hasValidPointer(root.currentPointerPosition)
                      ? target.mapToItem(bounds, root.currentPointerPosition.x,
                                         root.currentPointerPosition.y)
                      : Qt.point(-1, -1);
        const positionInBounds = root.resolvedPosition(
                    Qt.size(bounds.width, bounds.height),
                    targetRect,
                    Qt.size(tooltipWidth, tooltipHeight),
                    root.currentPlacement,
                    pointer);
        const position = bounds.mapToItem(tooltip.parent, positionInBounds.x, positionInBounds.y);
        tooltip.x = position.x;
        tooltip.y = position.y;
    }

    function clearCurrentState() {
        root.showTimer.stop();
        root.timeoutTimer.stop();
        root.currentTarget = null;
        root.currentPlacement = root.defaultPlacement;
        root.currentPointerPosition = Qt.point(-1, -1);
        root.currentTimeout = root.timeout;
        root.currentText = "";
    }

    function hide(target) {
        if (!target || root.currentTarget !== target)
            return;

        root.showTimer.stop();
        root.timeoutTimer.stop();
        root.popup.visible = false;
        root.clearCurrentState();
    }
}
