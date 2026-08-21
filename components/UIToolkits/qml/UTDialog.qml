/*
 * @file UTDialog.qml
 * @brief UIToolkits 的 UTDialog.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: root

    property string titleText: ""
    property bool titleBarVisible: true
    property bool titleBarDraggable: true
    // A few workflows own a non-cancellable write while their dialog is
    // visible.  Let those callers remove the close affordance without
    // changing the close behaviour of every other dialog.
    property bool closeButtonVisible: true
    property bool resizable: true
    property bool centerOnOpen: true
    property bool fullWindowOnOpen: false
    property real initialDialogWidth: 640
    property real initialDialogHeight: 480
    property real minimumDialogWidth: 240
    property real minimumDialogHeight: 160
    property alias dialogContent: contentHost.data

    signal titleCloseClicked()

    readonly property real effectiveMinimumWidth: parent ? Math.min(minimumDialogWidth, parent.width) : minimumDialogWidth
    readonly property real effectiveMinimumHeight: parent ? Math.min(minimumDialogHeight, parent.height) : minimumDialogHeight

    modal: false
    focus: true
    popupType: Popup.Item
    closePolicy: Popup.CloseOnEscape
    parent: Overlay.overlay
    padding: 4
    width: {
        if (!parent)
            return initialDialogWidth;
        if (fullWindowOnOpen)
            return Math.max(1, parent.width);
        return Math.max(1, Math.min(initialDialogWidth, parent.width - 32));
    }
    height: {
        if (!parent)
            return initialDialogHeight;
        if (fullWindowOnOpen)
            return Math.max(1, parent.height);
        return Math.max(1, Math.min(initialDialogHeight, parent.height - 32));
    }
    property bool clampingPosition: false

    function clampToParent() {
        if (!parent || clampingPosition)
            return;
        clampingPosition = true;
        x = Math.max(0, Math.min(x, Math.max(0, parent.width - width)));
        y = Math.max(0, Math.min(y, Math.max(0, parent.height - height)));
        clampingPosition = false;
    }

    function centerInParent() {
        if (!parent)
            return;
        x = Math.max(0, (parent.width - width) / 2);
        y = Math.max(0, (parent.height - height) / 2);
    }

    function resizeTo(nextWidth, nextHeight) {
        if (!parent)
            return;
        const maximumWidth = Math.max(1, parent.width - x);
        const maximumHeight = Math.max(1, parent.height - y);
        width = Math.min(maximumWidth, Math.max(Math.min(effectiveMinimumWidth, maximumWidth), nextWidth));
        height = Math.min(maximumHeight, Math.max(Math.min(effectiveMinimumHeight, maximumHeight), nextHeight));
    }

    function resizeFromLeft(nextLeft) {
        if (!parent)
            return;
        const right = Math.min(parent.width, x + width);
        const maximumLeft = right - effectiveMinimumWidth;
        const left = Math.max(0, Math.min(nextLeft, maximumLeft));
        x = left;
        width = right - left;
    }

    function clampSizeToParent() {
        if (!parent)
            return;
        if (fullWindowOnOpen) {
            x = 0;
            y = 0;
            return;
        }
        clampToParent();
        resizeTo(width, height);
        clampToParent();
    }

    onXChanged: clampToParent()
    onYChanged: clampToParent()
    onWidthChanged: clampToParent()
    onHeightChanged: clampToParent()
    onOpened: {
        if (fullWindowOnOpen) {
            x = 0;
            y = 0;
        } else if (centerOnOpen) {
            centerInParent();
        }
        UTLogger.debug("Dialog opened: " + titleText, "UIToolkits.Dialog");
    }
    onClosed: UTLogger.debug("Dialog closed: " + titleText, "UIToolkits.Dialog")

    Connections {
        target: root.parent

        function onWidthChanged() { root.clampSizeToParent(); }
        function onHeightChanged() { root.clampSizeToParent(); }
    }

    background: Rectangle {
        color: UTTheme.surfaceColor
        radius: UTTheme.radius || 8
        border.width: 1
        border.color: UTTheme.borderColor
    }

    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            UTTitleBar {
                visible: root.titleBarVisible
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                titleText: root.titleText
                windowObj: root
                draggable: root.titleBarDraggable && !root.fullWindowOnOpen
                doubleClickMaximizeEnabled: false
                minButtonVisible: false
                maxButtonVisible: false
                closeButtonVisible: root.closeButtonVisible
                onCloseClicked: {
                    root.titleCloseClicked();
                    root.close();
                }
            }

            Item {
                id: contentHost
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        Item {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 12
            visible: root.resizable && !root.fullWindowOnOpen
            z: 10

            HoverHandler {
                cursorShape: Qt.SizeHorCursor
            }

            DragHandler {
                target: null
                cursorShape: Qt.SizeHorCursor
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                property real initialLeft: 0
                onActiveChanged: if (active) initialLeft = root.x
                onTranslationChanged: if (active) root.resizeFromLeft(initialLeft + translation.x)
            }
        }

        Item {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 12
        visible: root.resizable && !root.fullWindowOnOpen
        z: 10

        HoverHandler {
            cursorShape: Qt.SizeHorCursor
        }

        DragHandler {
                target: null
                cursorShape: Qt.SizeHorCursor
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                property real initialWidth: 0
                onActiveChanged: if (active) initialWidth = root.width
                onTranslationChanged: if (active) root.resizeTo(initialWidth + translation.x, root.height)
            }
        }

        Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 12
        visible: root.resizable && !root.fullWindowOnOpen
        z: 10

        HoverHandler {
            cursorShape: Qt.SizeVerCursor
        }

        DragHandler {
                target: null
                cursorShape: Qt.SizeVerCursor
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                property real initialHeight: 0
                onActiveChanged: if (active) initialHeight = root.height
                onTranslationChanged: if (active) root.resizeTo(root.width, initialHeight + translation.y)
            }
        }

        Item {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: 24
            height: 24
            visible: root.resizable && !root.fullWindowOnOpen
            z: 11

            HoverHandler {
                cursorShape: Qt.SizeBDiagCursor
            }

            DragHandler {
                target: null
                cursorShape: Qt.SizeBDiagCursor
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                property real initialLeft: 0
                property real initialHeight: 0
                onActiveChanged: {
                    if (!active)
                        return;
                    initialLeft = root.x;
                    initialHeight = root.height;
                }
                onTranslationChanged: {
                    if (!active)
                        return;
                    root.resizeFromLeft(initialLeft + translation.x);
                    root.resizeTo(root.width, initialHeight + translation.y);
                }
            }
        }

        Item {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 24
        height: 24
        visible: root.resizable && !root.fullWindowOnOpen
        z: 11

        HoverHandler {
            cursorShape: Qt.SizeFDiagCursor
        }

        DragHandler {
                target: null
                cursorShape: Qt.SizeFDiagCursor
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                property real initialWidth: 0
                property real initialHeight: 0
                onActiveChanged: {
                    if (!active)
                        return;
                    initialWidth = root.width;
                    initialHeight = root.height;
                }
                onTranslationChanged: if (active) root.resizeTo(initialWidth + translation.x, initialHeight + translation.y)
            }
        }
    }
}
