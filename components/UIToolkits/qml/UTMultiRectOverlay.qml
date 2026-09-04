/*
 * @file UTMultiRectOverlay.qml
 * @brief UIToolkits 的 UTMultiRectOverlay.qml 组件。
 */

import QtQuick

Item {
    id: root

    // The overlay shares the image viewport, including when the image is
    // zoomed or panned.  Clip all ROI frames/labels/handles to that viewport;
    // otherwise a valid source rectangle can paint into the surrounding page
    // once its transformed coordinates fall outside the visible canvas.
    clip: true

    // Each group: { key, color, thickness, visible, rectangles: [{ key, x, y, width, height, label, color, thickness, hidden }] }
    property var groups: []
    /// Changes when the displayed image/data set changes; clears local edits.
    property var sessionKey: null
    property point imagePosition: Qt.point(0, 0)
    property real imageScale: 1
    property size imageSourceSize: Qt.size(0, 0)
    property string selectedKey: ""
    property string hoveredKey: ""
    property bool labelsVisible: true
    property bool selectedLabelsVisible: false
    property bool interactive: false
    property bool editable: false
    // A multi-view card can be selected before it becomes the active editor.
    // In that state the ROI is read-only but must still accept an interior
    // click; callers that need edge-only read-only hit testing leave this off.
    property bool allowInteriorSelection: false
    property bool creationEnabled: false
    property bool viewportInteractionEnabled: false
    property string createGroupKey: "general"
    property string createLabel: ""
    property real handleSize: 12
    property color defaultColor: "#b4d2ff"
    property real defaultThickness: 2
    // EditableDetectionRectItem in the Python parameter editor switches to
    // a uniform cyan selection pen, independent of each ROI's base color.
    property color selectedColor: "#00A8FF"
    property real selectedThickness: 2.2
    // Python QGraphics items use dashed component frames and, in CAD mode,
    // a translucent fill.  Keep these optional so existing overlays retain
    // their solid/transparent appearance.
    property string defaultLineStyle: "solid"
    property color defaultFillColor: "transparent"
    property real defaultFillOpacity: 0
    property var positionOverrides: ({})
    property var hiddenOverrides: ({})
    property var draftRectangle: null

    signal rectangleClicked(string key)
    signal rectangleSelected(string key)
    signal rectangleRightClicked(string key)
    signal rectangleUpdated(string key, var rectangle)
    signal rectangleEditFinished(string key, var rectangle)
    signal rectangleCreated(var rectangle)
    signal rectangleHidden(string key)
    signal allRectanglesShown()
    signal viewportPanStarted(real x, real y)
    signal viewportPanDelta(real deltaX, real deltaY)
    signal viewportPanFinished()
    signal viewportWheel(real x, real y, real delta)

    focus: editable

    function resetState() {
        positionOverrides = ({});
        hiddenOverrides = ({});
        draftRectangle = null;
        selectedKey = "";
        hoveredKey = "";
    }

    function setGroups(nextGroups, nextSessionKey) {
        groups = nextGroups || [];
        if (nextSessionKey !== undefined)
            sessionKey = nextSessionKey;
        else
            resetState();
    }

    // A caller that provides a session key owns the reset boundary.  Parameter
    // edits can then update rectangle geometry without clearing the overlay's
    // state twice and making the image panel appear to flicker.  Legacy users
    // without a key keep the original reset-on-data-change behavior.
    onGroupsChanged: {
        if (sessionKey === null)
            resetState()
    }
    onSessionKeyChanged: resetState()

    function imagePoint(viewX, viewY) {
        const scale = Math.max(imageScale, 0.0001);
        let x = (viewX - imagePosition.x) / scale;
        let y = (viewY - imagePosition.y) / scale;
        if (imageSourceSize.width > 0)
            x = Math.max(0, Math.min(imageSourceSize.width, x));
        if (imageSourceSize.height > 0)
            y = Math.max(0, Math.min(imageSourceSize.height, y));
        return Qt.point(x, y);
    }

    function setPositionOverride(key, rectangle) {
        const next = Object.assign({}, positionOverrides);
        next[key] = rectangle;
        positionOverrides = next;
        rectangleUpdated(key, rectangle);
    }

    function hideRectangle(key) {
        if (key.length === 0)
            return;
        const next = Object.assign({}, hiddenOverrides);
        next[key] = true;
        hiddenOverrides = next;
        if (selectedKey === key)
            selectedKey = "";
        if (hoveredKey === key)
            hoveredKey = "";
        rectangleHidden(key);
        UTLogger.debug("Rectangle hidden: " + key, "UIToolkits.MultiRectOverlay");
    }

    function showAllRectangles() {
        hiddenOverrides = ({});
        allRectanglesShown();
    }

    function updateDraft(point) {
        if (!draftRectangle)
            return;
        const x1 = Math.min(draftRectangle.startX, point.x);
        const y1 = Math.min(draftRectangle.startY, point.y);
        draftRectangle = {
            startX: draftRectangle.startX,
            startY: draftRectangle.startY,
            x: x1,
            y: y1,
            width: Math.abs(point.x - draftRectangle.startX),
            height: Math.abs(point.y - draftRectangle.startY)
        };
    }

    function finishDraft() {
        if (!draftRectangle)
            return;
        const draft = draftRectangle;
        draftRectangle = null;
        if (draft.width < 2 || draft.height < 2)
            return;
        rectangleCreated({
            key: "draft:" + Date.now(),
            x: Math.round(draft.x),
            y: Math.round(draft.y),
            width: Math.round(draft.width),
            height: Math.round(draft.height),
            label: createLabel,
            groupKey: createGroupKey
        });
        UTLogger.info("Rectangle created", "UIToolkits.MultiRectOverlay");
    }

    function transformRectangle(rectangle, handle, point) {
        let x1 = rectangle.x;
        let y1 = rectangle.y;
        let x2 = rectangle.x + rectangle.width;
        let y2 = rectangle.y + rectangle.height;
        if (handle === "tl") {
            x1 = point.x;
            y1 = point.y;
        } else if (handle === "tr") {
            x2 = point.x;
            y1 = point.y;
        } else if (handle === "br") {
            x2 = point.x;
            y2 = point.y;
        } else if (handle === "bl") {
            x1 = point.x;
            y2 = point.y;
        } else {
            x1 += point.x - rectangle.startX;
            x2 += point.x - rectangle.startX;
            y1 += point.y - rectangle.startY;
            y2 += point.y - rectangle.startY;
            if (imageSourceSize.width > 0) {
                const dx = x1 < 0 ? -x1 : (x2 > imageSourceSize.width ? imageSourceSize.width - x2 : 0);
                x1 += dx;
                x2 += dx;
            }
            if (imageSourceSize.height > 0) {
                const dy = y1 < 0 ? -y1 : (y2 > imageSourceSize.height ? imageSourceSize.height - y2 : 0);
                y1 += dy;
                y2 += dy;
            }
        }
        if (imageSourceSize.width > 0) {
            x1 = Math.max(0, Math.min(imageSourceSize.width, x1));
            x2 = Math.max(0, Math.min(imageSourceSize.width, x2));
        }
        if (imageSourceSize.height > 0) {
            y1 = Math.max(0, Math.min(imageSourceSize.height, y1));
            y2 = Math.max(0, Math.min(imageSourceSize.height, y2));
        }
        return {
            x: Math.round(Math.min(x1, x2)),
            y: Math.round(Math.min(y1, y2)),
            width: Math.round(Math.abs(x2 - x1)),
            height: Math.round(Math.abs(y2 - y1))
        };
    }

    Keys.onPressed: function (event) {
        if (!editable)
            return;
        const targetKey = hoveredKey.length > 0 ? hoveredKey : selectedKey;
        if (event.key === Qt.Key_S && targetKey.length > 0) {
            hideRectangle(targetKey);
            event.accepted = true;
        } else if (event.key === Qt.Key_W) {
            showAllRectangles();
            event.accepted = true;
        }
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        enabled: root.editable && (root.creationEnabled || root.viewportInteractionEnabled)
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        preventStealing: true
        scrollGestureEnabled: false
        property bool panning: false
        property real startX: 0
        property real startY: 0
        // A creation-only overlay cannot pan the viewport, so an open hand
        // suggests an unavailable gesture. Keep the hand pair only for
        // overlays that actually expose viewport panning; otherwise use the
        // normal arrow while the operator prepares a new rectangle.
        cursorShape: panning ? Qt.ClosedHandCursor :
                               (root.viewportInteractionEnabled ? Qt.OpenHandCursor : Qt.ArrowCursor)

        onPressed: function (mouse) {
            root.forceActiveFocus();
            const isPanGesture = mouse.button === Qt.MiddleButton
                                  || (mouse.button === Qt.LeftButton && !(mouse.modifiers & Qt.ControlModifier));
            if (isPanGesture && root.viewportInteractionEnabled) {
                panning = true;
                startX = mouse.x;
                startY = mouse.y;
                root.viewportPanStarted(mouse.x, mouse.y);
                return;
            }
            if (mouse.button !== Qt.LeftButton || !(mouse.modifiers & Qt.ControlModifier) || !root.creationEnabled) {
                mouse.accepted = false;
                return;
            }
            const point = root.imagePoint(mouse.x, mouse.y);
            root.draftRectangle = { startX: point.x, startY: point.y, x: point.x, y: point.y, width: 0, height: 0 };
        }
        onPositionChanged: function (mouse) {
            if (panning && pressed)
                root.viewportPanDelta(mouse.x - startX, mouse.y - startY);
            else if (pressed && root.draftRectangle)
                root.updateDraft(root.imagePoint(mouse.x, mouse.y));
        }
        onReleased: {
            if (panning) {
                panning = false;
                root.viewportPanFinished();
            } else {
                root.finishDraft();
            }
        }
        onWheel: function (wheel) {
            if (root.viewportInteractionEnabled) {
                root.viewportWheel(wheel.x, wheel.y, wheel.angleDelta.y);
                wheel.accepted = true;
            } else {
                wheel.accepted = false;
            }
        }
    }

    Repeater {
        model: root.groups

        delegate: Item {
            required property var modelData

            readonly property var group: modelData || ({})
            visible: group.visible !== false

            Repeater {
                model: group.rectangles || []

                delegate: Rectangle {
                    id: rectItem

                    required property var modelData
                    required property int index

                    // Do not resolve the overlay's QML id from methods that
                    // are invoked by a nested Repeater/Loader delegate.  Qt
                    // can lose that lexical id context there (the runtime
                    // symptom was "ReferenceError: root is not defined" on
                    // rectangleEditFinished).  Keep an explicit object
                    // reference on the rectangle instead.
                    // This component intentionally uses the top-level root
                    // for image transforms, selection and edit signals.  Do
                    // not compile it as a Bound delegate: Qt 6.10 may resolve
                    // the outer `root` id as null in nested Repeaters, which
                    // collapses every child ROI to 0x0 while self still draws.
                    property var overlayRoot: root

                    readonly property string rectKey: String(modelData.key || modelData.id || "")
                    // Detection area ids may be persisted as
                    // "componentRecordId:areaName". Python displays only the
                    // area name in the upper-left label.
                    readonly property string displayKey: {
                        if (modelData.displayKey !== undefined && String(modelData.displayKey).length > 0)
                            return String(modelData.displayKey);
                        const separator = rectKey.lastIndexOf(":");
                        return separator >= 0 ? rectKey.slice(separator + 1) : rectKey;
                    }
                    readonly property real sourceX: Number(modelData.x || 0)
                    readonly property real sourceY: Number(modelData.y || 0)
                    readonly property real sourceWidth: Number(modelData.width || modelData.w || 0)
                    readonly property real sourceHeight: Number(modelData.height || modelData.h || 0)
                    property var editOverride: null
                    readonly property var override: editOverride || (overlayRoot ? overlayRoot.positionOverrides[rectKey] : null)
                    readonly property real displayX: override ? override.x : sourceX
                    readonly property real displayY: override ? override.y : sourceY
                    readonly property real displayWidth: override ? override.width : sourceWidth
                    readonly property real displayHeight: override ? override.height : sourceHeight
                    readonly property real rectArea: Math.max(0, displayWidth * displayHeight)

                    function resolvedColor() {
                        const modelColor = modelData ? modelData.color : null;
                        const groupColor = group ? group.color : null;
                        const defaultColor = overlayRoot ? overlayRoot.defaultColor : null;
                        return modelColor || groupColor || defaultColor || "white";
                    }

                    function resolvedThickness() {
                        const modelThickness = modelData ? modelData.thickness : null;
                        const groupThickness = group ? group.thickness : null;
                        const defaultThickness = overlayRoot ? overlayRoot.defaultThickness : null;
                        const value = modelThickness || groupThickness || defaultThickness;
                        return value === undefined || value === null ? 1 : Number(value);
                    }

                    function resolvedLineStyle() {
                        const value = modelData && modelData.lineStyle !== undefined
                                ? modelData.lineStyle
                                : (group && group.lineStyle !== undefined ? group.lineStyle : (overlayRoot ? overlayRoot.defaultLineStyle : "solid"));
                        return String(value || "solid").toLowerCase();
                    }

                    function resolvedFillColor() {
                        const value = modelData && modelData.fillColor !== undefined
                                ? modelData.fillColor
                                : (group && group.fillColor !== undefined ? group.fillColor : (overlayRoot ? overlayRoot.defaultFillColor : "transparent"));
                        return value || "transparent";
                    }

                    function resolvedFillOpacity() {
                        // A delegate can briefly outlive its overlay during a
                        // model reset.  Resolve the local/group value first
                        // and use a literal fallback instead of dereferencing
                        // a transient null root (which used to spam QML
                        // warnings during multi-view refresh).
                        const value = modelData && modelData.fillOpacity !== undefined
                                ? modelData.fillOpacity
                                : (group && group.fillOpacity !== undefined ? group.fillOpacity : 0);
                        return Math.max(0, Math.min(1, Number(value || 0)));
                    }

                    function resolvedStackingOrder() {
                        const key = rectKey.toLowerCase();
                        // Multi-view renders body-self and child ROIs in one
                        // overlay, unlike single-view's two z-separated
                        // overlays.  Keep body-self at the bottom explicitly:
                        // selected-weight must never lift the enclosing body
                        // rectangle above a pin/character/polarity child,
                        // otherwise the body MouseArea swallows child clicks.
                        let value = 20;
                        if (key === "body-self" || key === "self")
                            value = 5;
                        else if (key.indexOf("solder_detection") >= 0 || key.indexOf("detection_rect") >= 0
                                || key.indexOf("solder_area_") >= 0)
                            value = 30;
                        else if (key.indexOf("margin") >= 0 || key.indexOf("polarity") >= 0 || key.indexOf("char") >= 0)
                            value = 25;
                        else if (key.indexOf("pad") >= 0)
                            value = 20;
                        // Python sorts equal-category overlays by area so the
                        // smallest box is on top and can be selected when
                        // rectangles overlap.  Keep the repeater index as a
                        // deterministic final tie-breaker.
                        return value + (selected ? 10 : 0) - rectArea / 1000000
                                + (100000 - index) / 1000000000;
                    }

                    readonly property color rectColor: resolvedColor()
                    readonly property real rectThickness: resolvedThickness()
                    readonly property string rectLineStyle: resolvedLineStyle()
                    readonly property color rectFillColor: resolvedFillColor()
                    readonly property real rectFillOpacity: resolvedFillOpacity()
                    readonly property bool selected: rectKey.length > 0 && overlayRoot && rectKey === overlayRoot.selectedKey
                    readonly property bool hovered: rectKey.length > 0 && overlayRoot && rectKey === overlayRoot.hoveredKey
                    readonly property string label: String(modelData.label || modelData.name || "")
                    readonly property string recognizedText: String(modelData.recognizedText || "")
                    readonly property bool hidden: modelData.hidden === true
                                                   || (overlayRoot && overlayRoot.hiddenOverrides[rectKey] === true)
                    readonly property bool rectangleEditable: !!overlayRoot && overlayRoot.editable
                                                                && modelData.editable !== false
                    property string editHandle: ""
                    property var editStart: null

                    function select() {
                        if (!overlayRoot)
                            return;
                        overlayRoot.selectedKey = rectKey;
                        overlayRoot.forceActiveFocus();
                        overlayRoot.rectangleSelected(rectKey);
                        overlayRoot.rectangleClicked(rectKey);
                    }

                    function beginEdit(handle, viewPoint) {
                        if (!overlayRoot)
                            return;
                        select();
                        overlayRoot.forceActiveFocus();
                        editHandle = handle;
                        const point = overlayRoot.imagePoint(viewPoint.x, viewPoint.y);
                        editStart = { x: displayX, y: displayY, width: displayWidth, height: displayHeight, startX: point.x, startY: point.y };
                    }

                    function updateEdit(viewPoint) {
                        if (editHandle.length === 0 || !editStart)
                            return;
                        const next = overlayRoot.transformRectangle(editStart, editHandle, overlayRoot.imagePoint(viewPoint.x, viewPoint.y));
                        if (next.width >= 2 && next.height >= 2)
                            editOverride = next;
                    }

                    function finishEdit() {
                        if (editHandle.length > 0 && override) {
                            const finished = override;
                            if (overlayRoot) {
                                overlayRoot.setPositionOverride(rectKey, finished);
                                overlayRoot.rectangleEditFinished(rectKey, finished);
                            }
                            UTLogger.debug("Rectangle edited: " + rectKey, "UIToolkits.MultiRectOverlay");
                        }
                        editOverride = null;
                        editHandle = "";
                        editStart = null;
                    }

                    x: overlayRoot ? overlayRoot.imagePosition.x + displayX * overlayRoot.imageScale : 0
                    y: overlayRoot ? overlayRoot.imagePosition.y + displayY * overlayRoot.imageScale : 0
                    width: overlayRoot ? displayWidth * overlayRoot.imageScale : 0
                    height: overlayRoot ? displayHeight * overlayRoot.imageScale : 0
                    z: resolvedStackingOrder()
                    visible: !hidden && displayWidth > 0 && displayHeight > 0
                    color: rectFillOpacity > 0 ? Qt.rgba(rectFillColor.r, rectFillColor.g,
                                                         rectFillColor.b, rectFillOpacity * rectFillColor.a) : "transparent"
                    // Rectangle has no dashed border primitive.  The Canvas
                    // below draws dashed frames while retaining this item as
                    // the stable hit target for editing.
                    border.color: !selected && (rectLineStyle === "dash" || rectLineStyle === "dashed")
                                  ? "transparent" : (selected ? overlayRoot.selectedColor : rectColor)
                    border.width: !selected && (rectLineStyle === "dash" || rectLineStyle === "dashed")
                                  ? 0 : (selected ? Math.max(1, overlayRoot.selectedThickness) : Math.max(1, rectThickness))

                    Canvas {
                        id: dashCanvas
                        anchors.fill: parent
                        visible: !rectItem.selected && (rectItem.rectLineStyle === "dash" || rectItem.rectLineStyle === "dashed")
                        contextType: "2d"
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            ctx.strokeStyle = rectItem.selected ? rectItem.overlayRoot.selectedColor : rectItem.rectColor;
                            ctx.lineWidth = rectItem.selected ? Math.max(1, rectItem.overlayRoot.selectedThickness) : Math.max(1, rectItem.rectThickness);
                            const dash = Math.max(4, ctx.lineWidth * 2.5);
                            ctx.setLineDash([dash, dash]);
                            const inset = ctx.lineWidth / 2;
                            ctx.strokeRect(inset, inset, Math.max(0, width - ctx.lineWidth), Math.max(0, height - ctx.lineWidth));
                        }
                        Component.onCompleted: dashCanvas.requestPaint()
                    }

                    onRectColorChanged: if (dashCanvas.visible) dashCanvas.requestPaint()
                    onRectThicknessChanged: if (dashCanvas.visible) dashCanvas.requestPaint()
                    onRectLineStyleChanged: if (dashCanvas.visible) dashCanvas.requestPaint()
                    onSelectedChanged: if (dashCanvas.visible) dashCanvas.requestPaint()
                    onWidthChanged: if (dashCanvas.visible) dashCanvas.requestPaint()
                    onHeightChanged: if (dashCanvas.visible) dashCanvas.requestPaint()

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        anchors.top: parent.top
                        anchors.topMargin: 3
                        visible: !!rectItem.overlayRoot && parent.label.length > 0
                                 && (rectItem.overlayRoot.labelsVisible
                                     || (rectItem.overlayRoot.selectedLabelsVisible && rectItem.selected))
                        text: rectItem.selected ? rectItem.displayKey : parent.label
                        color: parent.selected ? rectItem.overlayRoot.selectedColor : parent.rectColor
                        font.pixelSize: 12
                        font.family: UTTheme.fontFamily
                        font.weight: parent.selected ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 3
                        visible: rectItem.recognizedText.length > 0
                        width: characterLabel.implicitWidth + 8
                        height: characterLabel.implicitHeight + 4
                        color: "#A0000000"
                        radius: 2
                        z: 3

                        Text {
                            id: characterLabel
                            anchors.centerIn: parent
                            text: rectItem.recognizedText
                            color: "#FFF176"
                            font.pixelSize: 13
                            font.bold: true
                            font.family: UTTheme.fontFamily
                        }
                    }

                    Loader {
                        active: rectItem.rectangleEditable && rectItem.selected
                        anchors.fill: rectItem
                        sourceComponent: handlesComponent
                    }

                    Component {
                        id: handlesComponent

                        Repeater {
                            model: ["tl", "tr", "br", "bl"]

                        delegate: Rectangle {
                            required property string modelData

                            readonly property string handleName: modelData
                            width: rectItem.overlayRoot ? rectItem.overlayRoot.handleSize : 0
                            height: rectItem.overlayRoot ? rectItem.overlayRoot.handleSize : 0
                            x: handleName === "tl" || handleName === "bl" ? -width / 2 : parent.width - width / 2
                            y: handleName === "tl" || handleName === "tr" ? -height / 2 : parent.height - height / 2
                            visible: rectItem.rectangleEditable && rectItem.selected
                            color: rectItem.selected ? rectItem.overlayRoot.selectedColor : rectItem.rectColor
                            border.color: "#101010"
                            border.width: 1
                            z: 2

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                cursorShape: handleName === "tl" || handleName === "br" ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor

                                onPressed: function (mouse) {
                                    const point = parent.mapToItem(rectItem.overlayRoot, mouse.x, mouse.y);
                                    rectItem.beginEdit(handleName, point);
                                }
                                onPositionChanged: function (mouse) {
                                    if (pressed) {
                                        const point = parent.mapToItem(rectItem.overlayRoot, mouse.x, mouse.y);
                                        rectItem.updateEdit(point);
                                    }
                                }
                                onReleased: rectItem.finishEdit()
                            }
                        }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !!rectItem.overlayRoot && (rectItem.overlayRoot.interactive || rectItem.rectangleEditable)
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        // A parent card has a TapHandler for selecting the
                        // component.  Once the pointer is over a ROI, the
                        // ROI must retain the grab so multi-view selection and
                        // drag/resize are not stolen by that card handler.
                        preventStealing: true
                        hoverEnabled: enabled
                        cursorShape: rectItem.editHandle.length > 0 ? Qt.ClosedHandCursor :
                                     (rectItem.rectangleEditable ? Qt.SizeAllCursor : Qt.PointingHandCursor)

                        onEntered: {
                            if (rectItem.overlayRoot)
                                rectItem.overlayRoot.hoveredKey = rectItem.rectKey;
                        }
                        onExited: {
                            if (rectItem.overlayRoot && rectItem.overlayRoot.hoveredKey === rectItem.rectKey)
                                rectItem.overlayRoot.hoveredKey = "";
                        }
                        onPressed: function (mouse) {
                            // Read-only ROI rectangles often overlap.  Their
                            // full-area hit target made the topmost gray box
                            // swallow clicks intended for a lower box.  For
                            // non-editable rectangles, only the visible
                            // border is a hit target and interior clicks can
                            // propagate to the rectangle below.
                            // Editable rectangles must be selectable from their
                            // full area so single-view and multi-view expose
                            // identical drag/resize behavior.  Keep the
                            // border-only hit target only for read-only overlays
                            // to avoid overlapping gray boxes swallowing clicks.
                            if (!rectItem.rectangleEditable
                                    && (!rectItem.overlayRoot || !rectItem.overlayRoot.allowInteriorSelection)) {
                                const edgeDistance = Math.min(mouse.x, mouse.y,
                                                              width - mouse.x, height - mouse.y);
                                if (edgeDistance > 8) {
                                    mouse.accepted = false;
                                    return;
                                }
                            }
                            if (mouse.button === Qt.RightButton) {
                                rectItem.select();
                                rectItem.overlayRoot.rectangleRightClicked(rectItem.rectKey);
                                return;
                            }
                            if (rectItem.rectangleEditable && rectItem.overlayRoot.creationEnabled && (mouse.modifiers & Qt.ControlModifier)) {
                                const point = rectItem.mapToItem(rectItem.overlayRoot, mouse.x, mouse.y);
                                const imagePoint = rectItem.overlayRoot.imagePoint(point.x, point.y);
                                rectItem.overlayRoot.draftRectangle = { startX: imagePoint.x, startY: imagePoint.y, x: imagePoint.x, y: imagePoint.y, width: 0, height: 0 };
                                return;
                            }
                            if (rectItem.rectangleEditable) {
                                const point = rectItem.mapToItem(rectItem.overlayRoot, mouse.x, mouse.y);
                                rectItem.beginEdit("move", point);
                            } else {
                                rectItem.select();
                            }
                        }
                        onPositionChanged: function (mouse) {
                            if (pressed && rectItem.overlayRoot.draftRectangle) {
                                const point = rectItem.mapToItem(rectItem.overlayRoot, mouse.x, mouse.y);
                                rectItem.overlayRoot.updateDraft(rectItem.overlayRoot.imagePoint(point.x, point.y));
                            } else if (pressed && rectItem.editHandle.length > 0) {
                                const point = rectItem.mapToItem(rectItem.overlayRoot, mouse.x, mouse.y);
                                rectItem.updateEdit(point);
                            }
                        }
                        onReleased: {
                            if (rectItem.overlayRoot.draftRectangle)
                                rectItem.overlayRoot.finishDraft();
                            else
                                rectItem.finishEdit();
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        readonly property var draft: root.draftRectangle

        x: root.imagePosition.x + (draft ? draft.x : 0) * root.imageScale
        y: root.imagePosition.y + (draft ? draft.y : 0) * root.imageScale
        width: (draft ? draft.width : 0) * root.imageScale
        height: (draft ? draft.height : 0) * root.imageScale
        visible: draft !== null
        color: "transparent"
        border.color: "#ffffff"
        border.width: 2
    }
}
