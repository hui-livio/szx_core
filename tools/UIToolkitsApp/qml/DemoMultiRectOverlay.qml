/*
 * @file DemoMultiRectOverlay.qml
 * @brief UIToolkits 展示应用的 DemoMultiRectOverlay.qml 页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null
    property bool roiVisible: true
    property bool annotationVisible: true
    property var userRectangles: []
    property string selectedKey: "roi-1"

    readonly property var theme: UTTheme.theme
    readonly property var overlayGroups: [
        {
            key: "roi",
            color: "#42a5f5",
            thickness: 2,
            visible: root.roiVisible,
            rectangles: [
                { key: "roi-1", x: 74, y: 64, width: 190, height: 122, label: "Pad group A" },
                { key: "roi-2", x: 448, y: 212, width: 174, height: 138, label: "Pad group B" }
            ]
        },
        {
            key: "annotation",
            color: "#ec5dc5",
            thickness: 2,
            visible: root.annotationVisible,
            rectangles: [
                { key: "annotation-1", x: 280, y: 92, width: 220, height: 268, label: "IC body" },
                { key: "annotation-2", x: 544, y: 72, width: 146, height: 86, label: "Mark" }
            ]
        },
        {
            key: "draft",
            color: "#f5c84c",
            thickness: 2,
            rectangles: root.userRectangles
        }
    ]

    contentWidth: contentColumn.width
    contentHeight: contentColumn.implicitHeight

    ColumnLayout {
        id: contentColumn

        width: root.width - 14
        spacing: 20

        UTLabel {
            text: qsTr("MultiRectOverlay")
            textRole: UTLabel.TextRole.H2
            color: root.theme.primaryTextColor
        }

        UTLabel {
            text: qsTr("Grouped image-coordinate rectangles with labels, visibility and selection.")
            textRole: UTLabel.TextRole.Body
            color: root.theme.secondaryTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            UTButton {
                text: root.roiVisible ? qsTr("Hide ROI") : qsTr("Show ROI")
                buttonType: UTButton.ButtonType.Outlined
                onClicked: root.roiVisible = !root.roiVisible
            }

            UTButton {
                text: root.annotationVisible ? qsTr("Hide Annotation") : qsTr("Show Annotation")
                buttonType: UTButton.ButtonType.Outlined
                onClicked: root.annotationVisible = !root.annotationVisible
            }

            UTButton {
                text: qsTr("Clear Selection")
                buttonType: UTButton.ButtonType.Outlined
                onClicked: root.selectedKey = ""
            }

            Item {
                Layout.fillWidth: true
            }

            UTLabel {
                text: root.selectedKey.length > 0 ? qsTr("Selected: %1").arg(root.selectedKey) : qsTr("No selection")
                textRole: UTLabel.TextRole.Caption
                color: root.theme.secondaryTextColor
            }
        }

        Rectangle {
            id: preview

            property real zoom: 1
            property real panX: 0
            property real panY: 0
            property real panStartX: 0
            property real panStartY: 0

            function zoomAt(viewX, viewY, factor) {
                const oldScale = sourceImage.displayScale;
                const nextZoom = Math.max(0.2, Math.min(12, zoom * factor));
                const nextScale = sourceImage.baseScale * nextZoom;
                const imageX = (viewX - sourceImage.x) / oldScale;
                const imageY = (viewY - sourceImage.y) / oldScale;
                zoom = nextZoom;
                panX = viewX - imageX * nextScale - (width - 800 * nextScale) / 2;
                panY = viewY - imageY * nextScale - (height - 480 * nextScale) / 2;
            }

            Layout.fillWidth: true
            Layout.preferredHeight: 460
            color: "#121418"
            border.color: root.theme.borderColor
            border.width: 1
            radius: 8
            clip: true

            Item {
                id: sourceImage

                readonly property real baseScale: Math.min((preview.width - 48) / 800, (preview.height - 48) / 480)
                readonly property real displayScale: baseScale * preview.zoom
                width: 800 * displayScale
                height: 480 * displayScale
                x: (preview.width - width) / 2 + preview.panX
                y: (preview.height - height) / 2 + preview.panY

                Rectangle {
                    anchors.fill: parent
                    color: "#263238"
                    border.color: "#4f5b62"
                    border.width: 1
                }

                Repeater {
                    model: 7

                    delegate: Rectangle {
                        x: (index + 1) * sourceImage.width / 8
                        width: 1
                        height: sourceImage.height
                        color: "#ffffff"
                        opacity: 0.08
                    }
                }

                Repeater {
                    model: 4

                    delegate: Rectangle {
                        y: (index + 1) * sourceImage.height / 5
                        width: sourceImage.width
                        height: 1
                        color: "#ffffff"
                        opacity: 0.08
                    }
                }
            }

            UTMultiRectOverlay {
                anchors.fill: parent
                groups: root.overlayGroups
                imagePosition: Qt.point(sourceImage.x, sourceImage.y)
                imageScale: sourceImage.displayScale
                imageSourceSize: Qt.size(800, 480)
                selectedKey: root.selectedKey
                interactive: true
                editable: true
                creationEnabled: true
                viewportInteractionEnabled: true
                createGroupKey: "draft"
                createLabel: qsTr("New area")

                onRectangleClicked: function (key) {
                    root.selectedKey = key;
                    if (root.toast)
                        root.toast.showToast(qsTr("Selected %1").arg(key), UTToast.ToastType.Info, 1200);
                }
                onRectangleRightClicked: function (key) {
                    if (root.toast)
                        root.toast.showToast(qsTr("Right-clicked %1").arg(key), UTToast.ToastType.Info, 1200);
                }
                onRectangleCreated: function (rectangle) {
                    root.userRectangles = root.userRectangles.concat([rectangle]);
                    root.selectedKey = rectangle.key;
                }
                onViewportPanStarted: function () {
                    preview.panStartX = preview.panX;
                    preview.panStartY = preview.panY;
                }
                onViewportPanDelta: function (deltaX, deltaY) {
                    preview.panX = preview.panStartX + deltaX;
                    preview.panY = preview.panStartY + deltaY;
                }
                onViewportWheel: function (x, y, delta) {
                    preview.zoomAt(x, y, delta > 0 ? 1.12 : 1 / 1.12);
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                anchors.horizontalCenter: parent.horizontalCenter
                color: Qt.rgba(0, 0, 0, 0.45)
                radius: 4
                width: resolutionLabel.implicitWidth + 16
                height: resolutionLabel.implicitHeight + 8
                z: 3

                Label {
                    id: resolutionLabel
                    anchors.centerIn: parent
                    text: qsTr("%1% | %2 × %3").arg(Math.round(preview.zoom * 100)).arg(800).arg(480)
                    font.pixelSize: 11
                    color: "#d8dce2"
                }
            }

        }

        UTLabel {
            text: qsTr("Wheel zooms at the cursor; middle-drag or blank-canvas left-drag pans. Drag a selected rectangle to move it, drag a corner handle to resize it. Ctrl-drag creates a rectangle; S hides the hovered rectangle and W shows all.")
            textRole: UTLabel.TextRole.Caption
            color: root.theme.secondaryTextColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
}
