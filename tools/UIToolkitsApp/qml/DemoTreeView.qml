/*
 * @file DemoTreeView.qml
 * @brief UIToolkits 展示应用的 DemoTreeView.qml 页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
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
        spacing: 16

        UTLabel {
            text: "Tree View · 过滤"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        // ── 过滤输入 ────────────────────────────────────────────
        UTLineEdit {
            id: filterInput
            Layout.fillWidth: true
            placeholderText: "输入关键字过滤…"
            onTextChanged: filterTreeModel.filterText = text
        }

        // ── 操作按钮 ────────────────────────────────────────────
        RowLayout {
            spacing: 8
            UTButton {
                text: "展开全部"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: tree.expandAll()
            }
            UTButton {
                text: "折叠全部"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: tree.collapseAll()
            }
        }

        // ── 树 ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            color: theme.surfaceColor
            radius: theme.radius || 8
            border.width: 1
            border.color: theme.borderColor

            UTTreeView {
                id: tree
                anchors.fill: parent
                anchors.margins: 4
                model: filterTreeModel

                rowHeightProvider: function (row) {
                    let idx = tree.index(row, 0);
                    return idx.valid && tree.model.data(idx, Qt.UserRole + 5) ? 44 : 40;
                }

                delegate: DelegateChooser {
                    role: "hasChildren"

                    DelegateChoice {
                        roleValue: true
                        Rectangle {
                            required property int row
                            readonly property bool expanded: tree.isExpanded(row)
                            implicitWidth: tree.width - 16
                            implicitHeight: 44
                            radius: 10
                            color: expanded ? Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.12) : (hdrHover.hovered ? Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.06) : "transparent")

                            HoverHandler {
                                id: hdrHover
                            }

                            TapHandler {
                                onTapped: {
                                    if (tree.isExpanded(row))
                                        tree.collapseRecursively(row);
                                    else
                                        tree.expandRecursively(row);
                                    // 同步 selectionModel
                                    tree.selectionModel.setCurrentIndex(tree.index(row, 0), ItemSelectionModel.ClearAndSelect);
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: 28
                                    radius: 6
                                    color: Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.2)
                                    Text {
                                        anchors.centerIn: parent
                                        text: String(model.title || "").charAt(0)
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                        color: theme.primaryColor
                                    }
                                }
                                Text {
                                    text: model.title || ""
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: theme.primaryTextColor
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    radius: 10
                                    color: Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.15)
                                    Text {
                                        anchors.centerIn: parent
                                        text: tree.isExpanded(row) ? "▾" : "▸"
                                        font.pixelSize: 10
                                        color: theme.primaryColor
                                    }
                                }
                            }
                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }
                            }
                        }
                    }

                    DelegateChoice {
                        roleValue: false
                        Rectangle {
                            required property int row
                            readonly property bool selected: tree.currentItemData && tree.currentItemData.title === (model.title || "")
                            implicitWidth: tree.width - 36
                            implicitHeight: 40
                            radius: 8
                            color: selected ? Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.1) : (chdHover.hovered ? Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.05) : "transparent")

                            HoverHandler {
                                id: chdHover
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                Rectangle {
                                    Layout.preferredWidth: 4
                                    Layout.preferredHeight: 24
                                    radius: 2
                                    color: selected ? (theme.primaryColor) : (chdHover.hovered ? Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.4) : Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.15))
                                }
                                ColumnLayout {
                                    spacing: 2
                                    Text {
                                        text: model.title || ""
                                        font.pixelSize: 13
                                        font.weight: selected ? Font.DemiBold : Font.Normal
                                        color: selected ? (theme.primaryColor) : (theme.primaryTextColor)
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: "description for " + (model.title || "")
                                        font.pixelSize: 10
                                        color: theme.secondaryTextColor
                                        visible: selected || chdHover.hovered
                                        elide: Text.ElideRight
                                    }
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }

                            TapHandler {
                                onTapped: {
                                    let d = {
                                        title: model.title,
                                        icon: model.icon,
                                        extraData: model.extraData
                                    };
                                    tree.currentItemData = d;
                                    tree.itemClicked(d);
                                    // 同步 selectionModel
                                    tree.selectionModel.setCurrentIndex(tree.index(row, 0), ItemSelectionModel.ClearAndSelect);
                                    root.toast && root.toast.showToast("选中: " + (model.title || ""), UTToast.ToastType.Info, 1200);
                                }
                            }
                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }
                        }
                    }
                }

                onItemClicked: function (itemData) {
                    selectedTitle = itemData.title;
                }

                property string selectedTitle: ""
            }
        }

        // ── 选中提示 ────────────────────────────────────────────
        UTLabel {
            text: "当前选中: " + (tree.selectedTitle || "无")
            textRole: UTLabel.TextRole.Caption
            color: theme.primaryColor
        }

        // ── 强制刷新 ────────────────────────────────────────────
        Connections {
            target: filterTreeModel
            function onFilterTextChanged() {
                tree.forceLayout();
            }
        }
    }
}
