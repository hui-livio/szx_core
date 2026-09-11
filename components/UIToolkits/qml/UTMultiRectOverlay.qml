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
    // `selectedKey` is commonly bound to a page-level selection (CAD uses
    // `root.selectedAreaKey`).  Writing to a bound QML property from inside
    // the overlay silently removes that binding, which leaves multi-view
    // cards stuck on the first key they select.  Keep the public property
    // read/write for compatibility, but route local interaction through an
    // override and expose the effective value to delegates below.
    property string selectedKey: ""
    property string localSelectedKey: ""
    property bool localSelectionActive: false
    readonly property string activeSelectedKey: localSelectionActive ? localSelectedKey : selectedKey
    property string hoveredKey: ""
    property bool labelsVisible: true
    property bool selectedLabelsVisible: false
    // Optional page-owned semantic label resolver. Generic overlay users
    // continue to use the legacy label/name/displayKey fallback, while CAD
    // can translate algorithm keys (for example ``general``) to a
    // user-facing name before rendering the selected-frame badge.
    property var labelResolver: null
    // Optional owner-provided tooltip resolver. It receives the rectangle
    // model, persistence key and image-space geometry, allowing application
    // pages to expose domain metrics without coupling this reusable overlay
    // to PCB/CAD types.
    property var tooltipResolver: null
    // Persistence/algorithm keys are not labels. Keep this enabled by
    // default so a legacy DTO cannot leak values such as ``general`` or a
    // numeric database id into the canvas when a page forgot to install a
    // semantic resolver. Real/user supplied labels are still preserved.
    property bool sanitizeInternalLabels: true
    // Optional localized labels supplied by the owning page.  The fallback
    // strings below use this map first so a reusable UIToolkits component does
    // not lose the application's translation context (the QML `qsTr` call in
    // this module has a different context from the CAD page).
    property var semanticLabels: ({})
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
    // Geometry fingerprints let model updates replace only changed keys.  In
    // particular, a Body re-detect must not let an old drag override mask the
    // authoritative rectangle returned by the resolver.
    property var geometryFingerprints: ({})
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
        geometryFingerprints = ({});
        draftRectangle = null;
        // Keep an owner-provided selection across the session reset.  Only a
        // local (unbound) selection should be cleared here; masking a bound
        // key would make a component switch lose its highlight until the
        // page happened to write a different value.
        localSelectedKey = selectedKey;
        localSelectionActive = false;
        hoveredKey = "";
    }

    // An owner binding is authoritative whenever it changes.  This also
    // clears a local selection after a card/page switch sets the bound key to
    // an empty value, without assigning to `selectedKey` and breaking that
    // binding.
    onSelectedKeyChanged: {
        localSelectedKey = selectedKey;
        localSelectionActive = false;
    }

    function setGroups(nextGroups, nextSessionKey) {
        groups = nextGroups || [];
        if (nextSessionKey !== undefined)
            sessionKey = nextSessionKey;
        else
            reconcileGroups(groups);
    }

    function reconcileGroups(nextGroups) {
        const nextFingerprints = ({});
        const nextKeys = ({});
        const list = nextGroups || [];
        var rectangleCount = 0;
        var rectangleKeys = [];
        for (let gi = 0; gi < list.length; ++gi) {
            const group = list[gi] || {};
            const rectangles = group.rectangles || [];
            for (let ri = 0; ri < rectangles.length; ++ri) {
                const rectangle = rectangles[ri] || {};
                const key = String(rectangle.key || rectangle.id || "");
                if (key.length === 0)
                    continue;
                ++rectangleCount;
                rectangleKeys.push(key);
                nextKeys[key] = true;
                nextFingerprints[key] = [Number(rectangle.x || 0), Number(rectangle.y || 0),
                                         Number(rectangle.width || rectangle.w || 0), Number(rectangle.height || rectangle.h || 0)].join(":");
            }
        }
        const overrides = Object.assign({}, positionOverrides);
        const hidden = Object.assign({}, hiddenOverrides);
        for (const key in geometryFingerprints) {
            if (!nextKeys[key] || geometryFingerprints[key] !== nextFingerprints[key]) {
                delete overrides[key];
                delete hidden[key];
            }
        }
        positionOverrides = overrides;
        hiddenOverrides = hidden;
        geometryFingerprints = nextFingerprints;
        if (activeSelectedKey.length > 0 && !nextKeys[activeSelectedKey]) {
            // A model reset can briefly publish an empty rectangle list while
            // the page-level bound key is still valid.  Do not mask that
            // external selection, otherwise the key will not become selected
            // again when the new list arrives.  A local-only selection can be
            // cleared immediately because no external owner can restore it.
            if (localSelectionActive)
                localSelectedKey = "";
        }
        if (hoveredKey.length > 0 && !nextKeys[hoveredKey])
            hoveredKey = "";
        // This is intentionally emitted after reconciliation: it describes
        // the exact key set that the nested Repeaters will materialise.  When
        // the resolver/controller report two pads but the canvas still shows
        // more, this distinguishes a stale overlay model from an upstream
        // region list problem without logging image contents.
        if (typeof UTLogger !== "undefined")
            UTLogger.info("[TWO_ANGLE_OVERLAY_MODEL] group_count=" + list.length +
                          " rectangle_count=" + rectangleCount +
                          " keys=" + rectangleKeys.join(","), "PCBDetect.CadUI");
    }

    // A caller that provides a session key owns the reset boundary.  Parameter
    // edits can then update rectangle geometry without clearing the overlay's
    // state twice and making the image panel appear to flicker.  Legacy users
    // without a key keep the original reset-on-data-change behavior.
    onGroupsChanged: reconcileGroups(groups)
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
        if (activeSelectedKey === key) {
            localSelectedKey = "";
            localSelectionActive = true;
        }
        if (hoveredKey === key)
            hoveredKey = "";
        rectangleHidden(key);
        UTLogger.debug("Rectangle hidden: " + key, "UIToolkits.MultiRectOverlay");
    }

    function showAllRectangles() {
        hiddenOverrides = ({});
        allRectanglesShown();
    }

    // Return a semantic category for the algorithm/persistence key formats
    // used by the Python template editor. This intentionally uses token
    // boundaries rather than a broad `indexOf` check: a user supplied label
    // such as "general-purpose" must not be rewritten, while keys like
    // `left_general_0_1` and `component:general` must be hidden/mapped.
    function internalLabelCategory(value) {
        const raw = String(value || "").trim();
        let key = raw.toLowerCase();
        // A localized/semantic label is already safe to display.  In
        // particular, the English fallback strings contain spaces and would
        // otherwise be tokenized into `lead_box`/`pad_box`, making the
        // resolver reject its own result and occasionally fall through to a
        // raw persistence key.  Keep the known semantic forms out of the
        // internal-key classifier; Chinese translations naturally bypass
        // this list as well.
        if (/^(?:body self|lead box|pad box|solder|solder detection|cold-solder box|detection area|polarity|margin|slice)(?: \d+)?$/.test(key)
                || /^character(?: \d+)?$/.test(key))
            return "";
        if (key.length === 0)
            return "empty";
        // UUID/database identities are never user-facing labels, even when
        // no algorithm token is present.  Component-qualified algorithm keys
        // are handled below by classifying their suffix first.
        if (/^[0-9a-f]{8}-[0-9a-f-]{27,}$/i.test(key))
            return "numeric";
        const separator = key.lastIndexOf(":");
        const qualified = separator >= 0;
        if (separator >= 0)
            key = key.slice(separator + 1);
        // Python persistence keys use underscores, while a few legacy
        // snapshots (and human-entered fixtures) use hyphens/spaces.  Treat
        // all of them as token separators before classifying the key so
        // `left-general-0` cannot leak its internal name as a label.
        key = key.replace(/[\s-]+/g, "_");
        const hasToken = function (token) {
            return key === token || key.indexOf(token + "_") === 0
                    || key.endsWith("_" + token) || key.indexOf("_" + token + "_") >= 0;
        };
        if (/^\d+$/.test(key))
            return "numeric";
        if (hasToken("container_rect") || hasToken("self_error_rect"))
            return "metadata";
        if (hasToken("body_self") || key === "body" || key === "self" || hasToken("self"))
            return "self";
        if (key === "char" || key === "character" || key.indexOf("char_") === 0)
            return "character";
        if (hasToken("polarity"))
            return "polarity";
        if (key === "solder" || key.indexOf("solder_detection") >= 0 || key.indexOf("detection_rect") >= 0
                || key.indexOf("solder_area_") >= 0 || key.indexOf("hor_left_solder_area_") >= 0
                || key.indexOf("hor_right_solder_area_") >= 0 || key.indexOf("ver_left_solder_area_") >= 0
                || key.indexOf("ver_right_solder_area_") >= 0)
            return "solder";
        if (hasToken("cold"))
            return "cold";
        if (hasToken("angle") || hasToken("general") || hasToken("pin")
                || hasToken("lead") || hasToken("detection") || hasToken("gap"))
            return "lead";
        if (hasToken("pad"))
            return "pad";
        if (hasToken("margin"))
            return "margin";
        if (hasToken("slice"))
            return "slice";
        return qualified ? "numeric" : "";
    }

    function isInternalLabel(value) {
        const category = internalLabelCategory(value);
        return category === "empty" || category === "numeric" || category === "metadata"
                || category === "self" || category === "character" || category === "polarity"
                || category === "solder" || category === "cold" || category === "pad"
                || category === "lead" || category === "margin" || category === "slice";
    }

    // Keep the persistence/display-key boundary unconditional.  The
    // `sanitizeInternalLabels` switch is retained for source compatibility,
    // but it must never make an algorithm key user-visible: older callers can
    // still opt out of the semantic fallback while raw values such as
    // `general`, `angle_0` or a component id are rejected below.  This is
    // especially important for the selected-label path, which used to read
    // `displayKey` directly and could bypass the page resolver.
    function isUserFacingLabel(value) {
        const candidate = String(value || "").trim();
        return candidate.length > 0 && !isInternalLabel(candidate);
    }

    // The CAD controller keeps presentation labels in English source form
    // when no C++ translator is installed.  Do not let that source text win
    // over the page-owned translation context.  This helper only recognizes
    // the finite set of semantic labels emitted by the controller; arbitrary
    // operator supplied labels remain untouched.
    function localizedSemanticLabel(value) {
        const raw = String(value || "").trim();
        if (raw.length === 0)
            return "";
        const key = raw.toLowerCase().replace(/[\s_-]+/g, " ").trim();
        let category = "";
        let source = "";
        if (/^body self(?: \d+)?$/.test(key)) {
            category = "self";
            source = "Body self";
        } else if (/^lead box(?: \d+)?$/.test(key)) {
            category = "lead";
            source = "Lead box";
        } else if (/^pad box(?: \d+)?$/.test(key)) {
            category = "pad";
            source = "Pad box";
        } else if (/^solder detection(?: \d+)?$/.test(key)) {
            category = "solder";
            source = "Solder detection";
        } else if (/^cold solder box(?: \d+)?$/.test(key)) {
            category = "cold";
            source = "Cold-solder box";
        } else if (/^detection area(?: \d+)?$/.test(key)) {
            category = "numeric";
            source = "Detection area";
        } else if (/^character(?: \d+)?$/.test(key)) {
            category = "character";
            source = "Character";
        } else if (/^polarity(?: \d+)?$/.test(key)) {
            category = "polarity";
            source = "Polarity";
        } else if (/^solder(?: \d+)?$/.test(key)) {
            category = "solder_region";
            source = "Solder";
        } else if (/^margin(?: \d+)?$/.test(key)) {
            category = "margin";
            source = "Margin";
        } else if (/^slice(?: \d+)?$/.test(key)) {
            category = "slice";
            source = "Slice";
        }
        if (category.length === 0)
            return "";
        const labels = root.semanticLabels || {};
        const translated = labels[category] !== undefined && labels[category] !== null
                ? String(labels[category]).trim() : "";
        const base = translated.length > 0 ? translated : qsTr(source);
        if (base.length === 0)
            return "";
        const suffix = key.slice(source.toLowerCase().length);
        return base + suffix;
    }

    function semanticCategoryIsKnown(value) {
        const category = String(value || "").trim().toLowerCase();
        return ["self", "body", "character", "char", "polarity", "polar",
                "solder", "solder_detection", "solder_region", "cold", "pad",
                "lead", "general", "angle", "pin", "margin", "bridge", "slice",
                "metadata", "container"].indexOf(category) >= 0;
    }

    // Last-resort semantic labels for legacy overlays that do not provide a
    // page resolver. CAD pages provide `labelResolver`, so these source
    // strings can still be translated by the application's translator when
    // the overlay is used in a localized context; importantly, the raw
    // internal key is never returned.
    function semanticFallbackLabel(modelData, rectKey, displayKey) {
        const data = modelData || {};
        const explicitCategory = String(data.semanticCategory || "").trim().toLowerCase();
        // A typed category is authoritative.  In particular, a controller
        // may expose ``semanticCategory=lead`` together with the English
        // source label ``Lead box``; accepting that field before applying
        // `semanticLabels` would make the selected badge locale-dependent.
        if (!semanticCategoryIsKnown(explicitCategory)) {
            const values = [data.displayLabel, data.semanticLabel, data.localizedLabel, data.userLabel];
            for (let i = 0; i < values.length; ++i) {
                const candidate = String(values[i] || "").trim();
                if (candidate.length > 0 && !isInternalLabel(candidate)) {
                    const localized = localizedSemanticLabel(candidate);
                    return localized.length > 0 ? localized : candidate;
                }
            }
        }
        // Metadata regions are intentionally filtered from the editable scene;
        // if a legacy caller still forwards one, do not invent a visible label.
        if (explicitCategory === "metadata" || explicitCategory === "container")
            return "";
        // `semanticCategory` is a presentation hint emitted by the typed CAD
        // DTO.  Keep it ahead of persistence identities so a legacy numeric
        // areaId cannot win category inference (and so `general` is always
        // rendered as the localized lead-box label).
        // Preview/trace DTOs from older providers may carry only `groupKey`
        // while `areaId` is a numeric database identity.  Include that typed
        // group in the fallback candidates so the selected label still maps
        // to a semantic name (for example `general` -> Lead box) instead of
        // exposing the numeric id or a generic blank label.
        // Legacy detection-area DTOs can omit every typed field and leave
        // only `label`/`name` (often the Python key `general`) plus a numeric
        // database id.  Include those fields after the explicit typed hints:
        // internalLabelCategory() maps algorithm keys to a semantic category,
        // while human-authored labels simply produce no category and remain
        // eligible for the explicit-label path above.  This keeps the raw key
        // hidden without degrading the selected badge to a generic fallback.
        const candidates = [data.semanticCategory, data.areaType, data.type,
                            data.groupKey, data.group, data.label, data.name,
                            data.displayName, data.displayLabel, data.fullKey, data.areaId,
                            data.displayKey, rectKey, displayKey];
        let category = "";
        if (explicitCategory === "body" || explicitCategory === "self")
            category = "self";
        else if (explicitCategory === "char" || explicitCategory === "character")
            category = "character";
        else if (explicitCategory === "polar" || explicitCategory === "polarity")
            category = "polarity";
        else if (explicitCategory === "solder" || explicitCategory === "solder_detection")
            category = "solder";
        else if (explicitCategory === "solder_region")
            category = "solder_region";
        else if (explicitCategory === "cold")
            category = "cold";
        else if (explicitCategory === "pad")
            category = "pad";
        else if (explicitCategory === "lead" || explicitCategory === "general"
                 || explicitCategory === "angle" || explicitCategory === "pin")
            category = "lead";
        else if (explicitCategory === "margin" || explicitCategory === "bridge")
            category = "margin";
        else if (explicitCategory === "slice")
            category = "slice";
        let numericCategory = "";
        for (let i = 0; i < candidates.length; ++i) {
            if (category.length > 0)
                break;
            const candidateCategory = internalLabelCategory(candidates[i]);
            // Numeric/qualified database identities are useful only as a
            // last-resort fallback.  A legacy DTO can expose a numeric
            // `fullKey` together with the real algorithm key in `displayKey`
            // (for example `general`); stopping at the numeric value would
            // incorrectly label that ROI as a generic detection area.
            if (candidateCategory === "numeric") {
                if (numericCategory.length === 0)
                    numericCategory = candidateCategory;
                continue;
            }
            if (candidateCategory && candidateCategory !== "empty") {
                category = candidateCategory;
                break;
            }
        }
        if (category.length === 0)
            category = numericCategory;
        const labels = root.semanticLabels || {};
        if (category.length > 0 && labels[category] !== undefined && labels[category] !== null) {
            const localized = String(labels[category]).trim();
            if (localized.length > 0)
                return localized;
        }
        switch (category) {
        case "self": return qsTr("Body self");
        case "character": return qsTr("Character");
        case "polarity": return qsTr("Polarity");
        case "solder": return qsTr("Solder detection");
        case "solder_region": return qsTr("Solder");
        case "cold": return qsTr("Cold-solder box");
        case "pad": return qsTr("Pad box");
        case "lead": return qsTr("Lead box");
        case "margin": return qsTr("Margin");
        case "slice": return qsTr("Slice");
        case "numeric": return qsTr("Detection area");
        case "metadata": return "";
        default: return "";
        }
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
        const targetKey = hoveredKey.length > 0 ? hoveredKey : activeSelectedKey;
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

                    readonly property string rectKey: String(modelData
                                                               ? (modelData.key || modelData.id || "")
                                                               : "")
                    // Detection area ids may be persisted as
                    // "componentRecordId:areaName". Keep the persistence
                    // identity separate from the semantic, user-facing label
                    // rendered below.
                    readonly property string displayKey: {
                        if (modelData && modelData.displayKey !== undefined && String(modelData.displayKey).length > 0)
                            return String(modelData.displayKey);
                        const separator = rectKey.lastIndexOf(":");
                        return separator >= 0 ? rectKey.slice(separator + 1) : rectKey;
                    }
                    readonly property real sourceX: Number(modelData ? (modelData.x || 0) : 0)
                    readonly property real sourceY: Number(modelData ? (modelData.y || 0) : 0)
                    readonly property real sourceWidth: Number(modelData ? (modelData.width || modelData.w || 0) : 0)
                    readonly property real sourceHeight: Number(modelData ? (modelData.height || modelData.h || 0) : 0)
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

                     // Delegates may receive a final binding evaluation while
                     // their parent overlay is being removed (for example
                     // during a multi-view model reset).  Keep selected
                     // styling self-contained so that transient teardown never
                     // dereferences a null overlay and emits a QML warning.
                     function resolvedSelectedColor() {
                         return overlayRoot ? overlayRoot.selectedColor : "#00A8FF";
                     }

                     function resolvedSelectedThickness() {
                         const value = overlayRoot ? overlayRoot.selectedThickness : 2.2;
                         return Math.max(1, Number(value || 2.2));
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

                     // Delegates can survive for one event-loop turn while a
                     // Repeater is being reset.  During that window
                     // ``overlayRoot`` may be null; never use that transient
                     // state as permission to render a persistence key.  Keep
                     // a small local guard in addition to the overlay helper
                     // so values such as ``general`` cannot leak through the
                     // selected-label path when the parent is unavailable.
                     function userFacingLabel(value) {
                         const candidate = String(value || "").trim();
                         if (candidate.length === 0)
                             return false;
                         if (overlayRoot && typeof overlayRoot.isUserFacingLabel === "function")
                             return overlayRoot.isUserFacingLabel(candidate);
                         const normalized = candidate.toLowerCase().replace(/[\s-]+/g, "_");
                         const separator = normalized.lastIndexOf(":");
                         const suffix = separator >= 0 ? normalized.slice(separator + 1) : normalized;
                         if (/^\d+$/.test(suffix) || /^[0-9a-f]{8}-[0-9a-f-]{27,}$/i.test(suffix))
                             return false;
                         const hasToken = function (token) {
                             return suffix === token || suffix.indexOf(token + "_") === 0
                                     || suffix.endsWith("_" + token)
                                     || suffix.indexOf("_" + token + "_") >= 0;
                         };
                         return !(hasToken("body_self") || suffix === "body" || suffix === "self"
                                  || suffix === "char" || suffix.indexOf("char_") === 0
                                  || hasToken("polarity") || hasToken("solder") || hasToken("pad")
                                  || hasToken("angle") || hasToken("general") || hasToken("pin")
                                  || hasToken("lead") || hasToken("detection") || hasToken("margin")
                                  || hasToken("slice") || hasToken("cold") || hasToken("gap")
                                  || hasToken("container_rect") || hasToken("self_error_rect"));
                     }

                    readonly property color rectColor: resolvedColor()
                    readonly property real rectThickness: resolvedThickness()
                    readonly property string rectLineStyle: resolvedLineStyle()
                    readonly property color rectFillColor: resolvedFillColor()
                    readonly property real rectFillOpacity: resolvedFillOpacity()
                    readonly property bool selected: rectKey.length > 0 && overlayRoot && rectKey === overlayRoot.activeSelectedKey
                    readonly property bool hovered: rectKey.length > 0 && overlayRoot && rectKey === overlayRoot.hoveredKey
                     // ``displayLabel``/``semanticLabel`` are presentation
                     // hints supplied by the controller.  The owning page
                     // resolver runs first so its active locale and typed
                     // category win; legacy DTOs then use the defensive
                     // explicit/fallback path below.  A persistence key is
                     // never a label: in particular, ``displayKey`` must not
                     // be used as a last-resort text value because it is
                     // exactly where Python's internal ``general`` detector
                     // group is stored.
                    readonly property string label: {
                        // Repeater delegates can survive one event-loop turn
                        // after a model reset with a null modelData.  Keep
                        // label evaluation side-effect free and avoid a
                        // transient warning while the new ROI list arrives.
                        if (!modelData)
                            return "";
                         const overlay = rectItem.overlayRoot;
                        const resolver = overlay ? overlay.labelResolver : null;
                         if (typeof resolver === "function") {
                            // A page resolver is authoritative, but protect
                            // the delegate from malformed legacy DTOs and
                            // from a resolver accidentally returning an
                            // internal persistence key.
                            try {
                                const resolved = resolver(modelData, rectKey, displayKey);
                                if (resolved !== undefined && resolved !== null) {
                                    const resolvedText = String(resolved).trim();
                                    // Never return a persistence/algorithm key
                                    // even when a legacy overlay explicitly
                                    // disables fallback sanitization.
                                     if (resolvedText.length > 0
                                             && rectItem.userFacingLabel(resolvedText)) {
                                         const localized = overlay && typeof overlay.localizedSemanticLabel === "function"
                                                 ? overlay.localizedSemanticLabel(resolvedText) : "";
                                         return localized.length > 0 ? localized : resolvedText;
                                     }
                                }
                            } catch (error) {
                                // Fall through to the defensive semantic
                                // mapping below; a transient delegate/model
                                // reset must not expose a raw key or spam the
                                // UI with an uncaught QML exception.
                             }
                         }
                         const typedCategory = overlay && typeof overlay.semanticCategoryIsKnown === "function"
                                 ? overlay.semanticCategoryIsKnown(modelData.semanticCategory)
                                 : (String(modelData.semanticCategory || "").trim().length > 0
                                    && String(modelData.semanticCategory || "").trim().toLowerCase() !== "unknown");
                         // A typed category owns presentation. Without one,
                         // preserve operator-supplied labels for compatibility.
                         if (!typedCategory) {
                             const explicitValues = [modelData.displayLabel, modelData.semanticLabel];
                             for (let i = 0; i < explicitValues.length; ++i) {
                                 const explicitCandidate = String(explicitValues[i] || "").trim();
                                 if (explicitCandidate.length > 0 && rectItem.userFacingLabel(explicitCandidate)) {
                                     const localized = overlay && typeof overlay.localizedSemanticLabel === "function"
                                             ? overlay.localizedSemanticLabel(explicitCandidate) : "";
                                     return localized.length > 0 ? localized : explicitCandidate;
                                 }
                             }
                         }
                         const values = [modelData.label, modelData.name, modelData.displayName,
                                         modelData.localizedLabel, modelData.userLabel];
                         if (!typedCategory) {
                             for (let i = 0; i < values.length; ++i) {
                                 const candidate = String(values[i] || "").trim();
                                 if (candidate.length > 0 && rectItem.userFacingLabel(candidate)) {
                                     const localized = overlay && typeof overlay.localizedSemanticLabel === "function"
                                             ? overlay.localizedSemanticLabel(candidate) : "";
                                     return localized.length > 0 ? localized : candidate;
                                 }
                             }
                         }
                         // Internal keys are always sanitized, even for
                         // legacy callers that explicitly disable the old
                         // opt-in flag.  Opting out may suppress the semantic
                         // fallback in those callers, but it must never make
                         // an algorithm/database identity visible to users.
                         if (overlay && typeof overlay.semanticFallbackLabel === "function")
                             return overlay.semanticFallbackLabel(modelData, rectKey, displayKey);
                         return "";
                     }
                    readonly property string recognizedText: String(modelData ? (modelData.recognizedText || "") : "")
                    readonly property bool hidden: !!modelData && modelData.hidden === true
                                                   || (overlayRoot && overlayRoot.hiddenOverrides[rectKey] === true)
                    readonly property bool rectangleEditable: !!overlayRoot && overlayRoot.editable
                                                                && modelData.editable !== false
                    property string editHandle: ""
                    property var editStart: null

                    function select() {
                        if (!overlayRoot)
                            return;
                        // Never assign to `selectedKey`: callers may bind it
                        // to a page-level selection property.  Such an
                        // assignment would destroy the binding and prevent
                        // later single/multi-view synchronization.
                        overlayRoot.localSelectedKey = rectKey;
                        overlayRoot.localSelectionActive = true;
                        overlayRoot.forceActiveFocus();
                        overlayRoot.rectangleSelected(rectKey);
                        overlayRoot.rectangleClicked(rectKey);
                    }

                    function updateTooltip(handler) {
                        const overlay = rectItem.overlayRoot;
                        if (!overlay || !handler || !handler.hovered || typeof overlay.tooltipResolver !== "function") {
                            UTTooltip.hide(rectItem);
                            return;
                        }
                        let text = "";
                        try {
                            const value = overlay.tooltipResolver(modelData, rectKey, displayKey,
                                                                 displayX, displayY, displayWidth, displayHeight);
                            text = String(value || "").trim();
                        } catch (error) {
                            text = "";
                        }
                        if (text.length === 0) {
                            UTTooltip.hide(rectItem);
                            return;
                        }
                        UTTooltip.update(rectItem, text, true, undefined, UTTooltip.Auto,
                                         handler.point ? handler.point.position : Qt.point(-1, -1));
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
                                   ? "transparent" : (selected ? rectItem.resolvedSelectedColor() : rectColor)
                     border.width: !selected && (rectLineStyle === "dash" || rectLineStyle === "dashed")
                                   ? 0 : (selected ? rectItem.resolvedSelectedThickness() : Math.max(1, rectThickness))

                    Canvas {
                        id: dashCanvas
                        anchors.fill: parent
                        visible: !rectItem.selected && (rectItem.rectLineStyle === "dash" || rectItem.rectLineStyle === "dashed")
                        contextType: "2d"
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            ctx.strokeStyle = rectItem.selected ? rectItem.resolvedSelectedColor() : rectItem.rectColor;
                            ctx.lineWidth = rectItem.selected ? rectItem.resolvedSelectedThickness() : Math.max(1, rectItem.rectThickness);
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
                        // Both selected and unselected states use the same
                        // semantic label.  The previous selected-only
                        // ``displayKey`` path leaked internal algorithm group
                        // names (for example ``general``) into the canvas.
                         // Apply the final identity boundary at the paint
                         // node as well.  A stale delegate or a legacy
                         // caller can otherwise bypass the resolver for one
                         // frame during a model reset; internal keys such as
                         // `general` must never become visible text.
                         text: rectItem.userFacingLabel(parent.label) ? parent.label : ""
                        color: parent.selected ? rectItem.resolvedSelectedColor() : parent.rectColor
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
                            color: rectItem.selected ? rectItem.resolvedSelectedColor() : rectItem.rectColor
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

                    HoverHandler {
                        id: tooltipHoverHandler
                        enabled: !!rectItem.overlayRoot && typeof rectItem.overlayRoot.tooltipResolver === "function"
                        onHoveredChanged: rectItem.updateTooltip(tooltipHoverHandler)
                        onPointChanged: {
                            if (hovered)
                                rectItem.updateTooltip(tooltipHoverHandler);
                        }
                    }

                    Component.onDestruction: UTTooltip.hide(rectItem)
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
