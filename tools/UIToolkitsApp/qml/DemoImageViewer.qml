/*
 * @file DemoImageViewer.qml
 * @brief UTImageViewer 演示页面
 */

// ========================================================================
// ========================================================================

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import UIToolkits

UTFlickable {
    id: root

    property var toast: null

    readonly property var theme: UTTheme.theme

    contentWidth: contentCol.width
    contentHeight: contentCol.implicitHeight

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 24

        // ── 标题 ──────────────────────────────────────────────────
        UTLabel {
            text: "ImageViewer"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "图片预览 · 拖拽平移 · 滚轮缩放"
            textRole: UTLabel.TextRole.Body
            color: theme.secondaryTextColor
        }

        // ── 工具栏 ────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UTButton {
                text: "选择本地图片"
                buttonType: UTButton.ButtonType.Filled
                onClicked: fileDialog.open()
            }

            UTButton {
                text: "加载示例图片"
                buttonType: UTButton.ButtonType.Tonal
                onClicked: {
                    viewer.loadFromUrl("https://upload.wikimedia.org/wikipedia/commons/thumb/4/47/PNG_transparency_demonstration_1.png/600px-PNG_transparency_demonstration_1.png");
                }
            }

            UTButton {
                text: "适应窗口"
                buttonType: UTButton.ButtonType.Outlined
                enabled: viewer.hasImage
                onClicked: viewer.fitToViewport()
            }

            UTButton {
                text: "清除图片"
                buttonType: UTButton.ButtonType.Outlined
                enabled: viewer.hasImage
                onClicked: viewer.clear()
            }

            Item {
                Layout.fillWidth: true
            }

            UTLabel {
                text: viewer.hasImage ? "缩放: %1% | %2 × %3".arg(Math.round(viewer.zoomLevel * 100)).arg(viewer.imageSourceSize.width).arg(viewer.imageSourceSize.height) : "未加载图片"
                textRole: UTLabel.TextRole.Caption
                color: theme.secondaryTextColor
            }
        }

        // ── 图片查看器 ────────────────────────────────────────────
        UTImageViewer {
            id: viewer
            Layout.fillWidth: true
            Layout.preferredHeight: 420
        }

        // ── 操作说明 ─────────────────────────────────────────────
        UTLabel {
            text: "操作说明"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 24

            ColumnLayout {
                spacing: 4

                UTLabel {
                    text: "🖱 滚轮"
                    textRole: UTLabel.TextRole.Caption
                    color: theme.primaryColor
                }
                UTLabel {
                    text: "以鼠标位置为中心缩放"
                    textRole: UTLabel.TextRole.Body
                    color: theme.secondaryTextColor
                }
            }

            ColumnLayout {
                spacing: 4

                UTLabel {
                    text: "✋ 左键拖拽"
                    textRole: UTLabel.TextRole.Caption
                    color: theme.primaryColor
                }
                UTLabel {
                    text: "平移图片"
                    textRole: UTLabel.TextRole.Body
                    color: theme.secondaryTextColor
                }
            }

            ColumnLayout {
                spacing: 4

                UTLabel {
                    text: "👆 双击"
                    textRole: UTLabel.TextRole.Caption
                    color: theme.primaryColor
                }
                UTLabel {
                    text: "适应窗口"
                    textRole: UTLabel.TextRole.Body
                    color: theme.secondaryTextColor
                }
            }
        }

        // ── 属性说明 ─────────────────────────────────────────────
        UTLabel {
            text: "属性面板"
            textRole: UTLabel.TextRole.H3
            color: theme.primaryTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UTButton {
                text: viewer.panEnabled ? "平移: 开" : "平移: 关"
                buttonType: viewer.panEnabled ? UTButton.ButtonType.Filled : UTButton.ButtonType.Outlined
                onClicked: viewer.panEnabled = !viewer.panEnabled
            }

            UTButton {
                text: viewer.zoomEnabled ? "缩放: 开" : "缩放: 关"
                buttonType: viewer.zoomEnabled ? UTButton.ButtonType.Filled : UTButton.ButtonType.Outlined
                onClicked: viewer.zoomEnabled = !viewer.zoomEnabled
            }

            UTButton {
                text: viewer.doubleClickFitEnabled ? "双击适应: 开" : "双击适应: 关"
                buttonType: viewer.doubleClickFitEnabled ? UTButton.ButtonType.Filled : UTButton.ButtonType.Outlined
                onClicked: viewer.doubleClickFitEnabled = !viewer.doubleClickFitEnabled
            }
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }

    // ── 文件对话框 ───────────────────────────────────────────────
    FileDialog {
        id: fileDialog
        title: "选择图片"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff)", "PNG (*.png)", "JPEG (*.jpg *.jpeg)", "BMP (*.bmp)", "All files (*)"]
        onAccepted: viewer.loadFromFile(String(selectedFile).replace("file:///", ""))
    }
}
