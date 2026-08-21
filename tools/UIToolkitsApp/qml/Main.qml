/*
 * @file Main.qml
 * @brief UIToolkits Demo Gallery — 左侧导航 + 右侧演示区
 */

// ========================================================================
// ========================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UIToolkits

UTAppWindow {
    id: window
    visible: false  // 延迟到 main.cc setupNativeWindow 之后显示，避免闪烁
    width: 1100
    height: 760
    title: "UIToolkits · Demo Gallery"

    // ── Nav model ─────────────────────────────────────────────────
    ListModel {
        id: navModel
        ListElement {
            name: "Buttons"
            category: "Basic"
        }
        ListElement {
            name: "Inputs"
            category: "Basic"
        }
        ListElement {
            name: "Selections"
            category: "Basic"
        }
        ListElement {
            name: "Display"
            category: "Basic"
        }
        ListElement {
            name: "Feedback"
            category: "Basic"
        }
        ListElement {
            name: "Pickers"
            category: "Basic"
        }
        ListElement {
            name: "Charts"
            category: "Basic"
        }
        ListElement {
            name: "Calendar"
            category: "Basic"
        }
        ListElement {
            name: "TreeView"
            category: "Basic"
        }
        ListElement {
            name: "Colors"
            category: "Basic"
        }
        ListElement {
            name: "TabBar"
            category: "Navigation"
        }
        ListElement {
            name: "FootBar"
            category: "Navigation"
        }
        ListElement {
            name: "Menu"
            category: "Navigation"
        }
        ListElement {
            name: "ImageViewer"
            category: "Media"
        }
        ListElement {
            name: "MultiRectOverlay"
            category: "Media"
        }
        ListElement {
            name: "Dialogs"
            category: "Feedback"
        }
    }

    // ── Toast overlay ─────────────────────────────────────────────
    UTToast {
        id: toastCtrl
        parent: window.contentItem
    }

    // ── Shortcut ──────────────────────────────────────────────────
    Shortcut {
        sequence: "Ctrl+T"
        context: Qt.ApplicationShortcut
        onActivated: window.nextTheme()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: window.contentMargin
        spacing: 0

        UTTitleBar {
            id: titleBar
            Layout.fillWidth: true
            implicitHeight: 42
            windowObj: window
            draggable: true
            titleText: window.title
            iconSource: appIconSource
            onMinimizeClicked: window.showMinimized()
            onMaximizeClicked: {
                window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized();
            }
            onCloseClicked: window.close()
        }

        // =================================================================
        // 主布局: 左侧导航 + 右侧内容
        // =================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── 左侧导航面板 ──────────────────────────────────────────
            Rectangle {
                id: navPanel
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                color: theme.surfaceColor

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // 导航标题
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56

                        UTLabel {
                            anchors.centerIn: parent
                            text: "UIToolkits"
                            textRole: UTLabel.TextRole.H4
                            color: theme.primaryColor
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: theme.borderColor
                    }

                    // 导航列表
                    ListView {
                        id: navList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: navModel
                        clip: true
                        currentIndex: 0

                        delegate: ItemDelegate {
                            id: navItem
                            width: ListView.view.width
                            implicitHeight: 44

                            readonly property bool isActive: ListView.isCurrentItem

                            contentItem: RowLayout {
                                spacing: 12

                                Item {
                                    Layout.preferredWidth: 4
                                }

                                Rectangle {
                                    Layout.preferredWidth: 3
                                    Layout.preferredHeight: 20
                                    radius: 2
                                    color: navItem.isActive ? (theme.primaryColor) : "transparent"
                                    visible: navItem.isActive
                                }

                                UTLabel {
                                    text: model.name
                                    textRole: UTLabel.TextRole.Body
                                    color: navItem.isActive ? (theme.primaryColor) : (theme.primaryTextColor)
                                    font.weight: navItem.isActive ? Font.DemiBold : Font.Normal
                                }
                            }

                            background: Rectangle {
                                color: navItem.isActive ? Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.08) : (navItem.hovered ? Qt.rgba(theme.primaryColor.r, theme.primaryColor.g, theme.primaryColor.b, 0.04) : "transparent")
                            }

                            onClicked: navList.currentIndex = index
                        }
                    }

                    // 底部主题切换
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48

                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: theme.borderColor
                            border.width: 1
                        }

                        UTButton {
                            anchors.centerIn: parent
                            text: "Toggle Theme (Ctrl+T)"
                            buttonType: UTButton.ButtonType.Text
                            font.pixelSize: 11
                            onClicked: window.nextTheme()
                        }
                    }
                }
            }

            // ── 分隔线 ────────────────────────────────────────────────
            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: theme.borderColor
            }

            // ── 右侧内容区 ────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: theme.backgroundColor

                StackLayout {
                    id: contentStack
                    anchors.fill: parent
                    anchors.margins: 24
                    currentIndex: navList.currentIndex

                    DemoButtons {
                        toast: toastCtrl
                    }
                    DemoInputs {
                        toast: toastCtrl
                    }
                    DemoSelections {
                        toast: toastCtrl
                    }
                    DemoDisplay {
                        toast: toastCtrl
                    }
                    DemoFeedback {
                        toast: toastCtrl
                    }
                    DemoPickers {
                        toast: toastCtrl
                    }
                    DemoCharts {
                        toast: toastCtrl
                    }
                    DemoCalendar {
                        toast: toastCtrl
                    }
                    DemoTreeView {
                        toast: toastCtrl
                    }
                    DemoColors {
                        toast: toastCtrl
                    }
                    DemoTabBar {
                        toast: toastCtrl
                    }
                    DemoFootBar {
                        toast: toastCtrl
                    }
                    DemoMenu {
                        toast: toastCtrl
                    }
                    DemoImageViewer {
                        toast: toastCtrl
                    }
                    DemoMultiRectOverlay {
                        toast: toastCtrl
                    }
                    DemoDialogs {
                        toast: toastCtrl
                    }
                }
            }
        }

        UTFootBar {
            Layout.fillWidth: true
            statusText: qsTr("Ready")
            statusTone: UTFootBar.StatusTone.Success
            centerText: qsTr("UIToolkits Demo Gallery")
            rightText: qsTr("v1.0")
        }
    }
}
