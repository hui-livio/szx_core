/*
 * @file DemoMenu.qml
 * @brief UIToolkits 展示应用的 DemoMenu.qml 页面。
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null

    readonly property var theme: UTTheme.theme

    property string menuResult: ""

    contentWidth: contentCol.width
    contentHeight: contentCol.implicitHeight

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 16

        UTLabel {
            text: "Menu"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "UTMenu — 主题化右键菜单 / 下拉菜单，支持 actionsModel 与 children 多级子菜单"
            textRole: UTLabel.TextRole.Caption
            color: theme.secondaryTextColor
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        // ── 右键菜单 ──────────────────────────────────────────────
        UTLabel {
            text: "右键菜单"
            textRole: UTLabel.TextRole.H4
            color: theme.primaryTextColor
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            color: theme.surfaceColor
            radius: theme.radius || 8
            border.width: 1
            border.color: theme.borderColor

            UTLabel {
                anchors.centerIn: parent
                text: "在此区域右键点击"
                textRole: UTLabel.TextRole.Body
                color: theme.secondaryTextColor
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onPressed: function (mouse) {
                    if (mouse.button === Qt.RightButton) {
                        contextMenu.popup(mouse.x, mouse.y);
                    }
                }
            }

            UTMenu {
                id: contextMenu

                actionsModel: [
                    {
                        text: "剪贴板",
                        children: [
                            {
                                id: "copy",
                                text: "复制"
                            },
                            {
                                id: "paste",
                                text: "粘贴"
                            },
                            {
                                text: "选择性粘贴",
                                children: [
                                    {
                                        id: "paste_text",
                                        text: "纯文本"
                                    },
                                    {
                                        id: "paste_image",
                                        text: "图片"
                                    }
                                ]
                            }
                        ]
                    },
                    {
                        text: "—"
                    },
                    {
                        text: "全选",
                        enabled: true
                    },
                    {
                        text: "删除",
                        enabled: false
                    }
                ]

                onActionSelected: function (action, indexPath) {
                    const path = indexPath.join(" > ");
                    menuResult = "右键菜单: " + action.text + " [" + path + "]";
                    root.toast && root.toast.showToast("右键: " + action.text, UTToast.ToastType.Info, 1000);
                }
            }
        }

        // ── 下拉菜单 ──────────────────────────────────────────────
        UTLabel {
            text: "下拉菜单"
            textRole: UTLabel.TextRole.H4
            color: theme.primaryTextColor
        }

        RowLayout {
            spacing: 12

            UTButton {
                text: "文件 ▼"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: fileMenu.popup(this, 0, this.height + 4)
            }

            UTButton {
                text: "编辑 ▼"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: editMenu.popup(this, 0, this.height + 4)
            }

            UTButton {
                text: "视图 ▼"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: viewMenu.popup(this, 0, this.height + 4)
            }
        }

        UTMenu {
            id: fileMenu
            actionsModel: [
                {
                    text: "新建",
                    children: [
                        {
                            id: "new_project",
                            text: "项目"
                        },
                        {
                            text: "文档",
                            children: [
                                {
                                    id: "new_text",
                                    text: "文本文档"
                                },
                                {
                                    id: "new_config",
                                    text: "配置文件"
                                }
                            ]
                        }
                    ]
                },
                {
                    text: "打开",
                    children: [
                        {
                            id: "open_file",
                            text: "选择文件…"
                        },
                        {
                            text: "最近打开",
                            children: [
                                {
                                    id: "recent_project_a",
                                    text: "Project-A"
                                },
                                {
                                    id: "recent_project_b",
                                    text: "Project-B"
                                }
                            ]
                        }
                    ]
                },
                {
                    text: "—"
                },
                {
                    text: "保存",
                    enabled: true
                },
                {
                    text: "另存为…",
                    enabled: true
                },
                {
                    text: "—"
                },
                {
                    text: "退出",
                    enabled: true
                }
            ]
            onActionSelected: function (action, indexPath) {
                const path = indexPath.join(" > ");
                menuResult = "文件: " + action.text + " [" + path + "]";
                root.toast && root.toast.showToast("文件: " + action.text, UTToast.ToastType.Info, 1000);
            }
        }

        UTMenu {
            id: editMenu
            actionsModel: [
                {
                    text: "撤销",
                    enabled: true
                },
                {
                    text: "重做",
                    enabled: false
                },
                {
                    text: "—"
                },
                {
                    text: "剪切",
                    enabled: true
                },
                {
                    text: "复制",
                    enabled: true
                },
                {
                    text: "粘贴",
                    enabled: true
                }
            ]
            onActionTriggered: function (index, text) {
                menuResult = "编辑: " + text;
                root.toast && root.toast.showToast("编辑: " + text, UTToast.ToastType.Info, 1000);
            }
        }

        UTMenu {
            id: viewMenu
            actionsModel: [
                {
                    text: "状态栏",
                    enabled: true,
                    checkable: true,
                    checked: true
                },
                {
                    text: "工具栏",
                    enabled: true,
                    checkable: true,
                    checked: true
                },
                {
                    text: "侧边栏",
                    enabled: true,
                    checkable: true,
                    checked: false
                },
                {
                    text: "—"
                },
                {
                    text: "全屏",
                    enabled: true,
                    checkable: true,
                    checked: false
                }
            ]
            onActionTriggered: function (index, text) {
                menuResult = "视图: " + text;
                root.toast && root.toast.showToast("视图: " + text, UTToast.ToastType.Info, 1000);
            }
        }

        // ── 结果提示 ──────────────────────────────────────────────
        UTLabel {
            text: "操作结果: " + (menuResult || "—")
            textRole: UTLabel.TextRole.Caption
            color: theme.primaryColor
            visible: menuResult !== ""
        }

        Item {
            Layout.preferredHeight: 8
        }
    }
}
