/*
 * @file DemoColors.qml
 * @brief UIToolkits 展示应用的 DemoColors.qml 页面。
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

    // 颜色定义列表
    readonly property var colorKeys: ["primaryColor", "focusColor", "backgroundColor", "surfaceColor", "primaryTextColor", "secondaryTextColor", "borderColor", "shadowColor", "titleBarColor", "surfaceButtonHoverColor", "surfaceButtonPressedColor", "filledButtonHoverColor", "filledButtonPressedColor", "tonalButtonHoverColor", "tonalButtonPressedColor", "textButtonHoverColor", "textButtonPressedColor", "surfaceButtonHoverBorderColor", "scrollbar.trackColor", "scrollbar.thumbNormalColor", "scrollbar.thumbHoverColor", "scrollbar.thumbPressedColor"]

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 24

        UTLabel {
            text: "Color Palette"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTLabel {
            text: "当前主题: " + (theme.name || "unknown") + " | 点击颜色块复制 Key"
            textRole: UTLabel.TextRole.Body
            color: theme.secondaryTextColor
        }

        // 颜色网格
        GridView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.ceil(colorGridModel.count / 4) * 140 + 20
            model: colorGridModel
            cellWidth: width / 4
            cellHeight: 140
            clip: true

            delegate: Item {
                width: GridView.view.cellWidth - 16
                height: GridView.view.cellHeight - 16

                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: theme.surfaceColor
                    border.width: 1
                    border.color: theme.borderColor

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        // 颜色预览
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            radius: 8
                            color: model.colorValue
                            border.width: 1
                            border.color: Qt.rgba(0, 0, 0, 0.1)

                            Text {
                                anchors.centerIn: parent
                                text: model.colorValue
                                font.pixelSize: 11
                                font.family: "Consolas, monospace"
                                color: {
                                    let c = Qt.color(model.colorValue);
                                    let luminance = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
                                    return luminance > 0.5 ? "#000000" : "#ffffff";
                                }
                            }
                        }

                        // 颜色名称
                        Text {
                            text: model.colorName
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            color: theme.primaryTextColor
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // 复制按钮
                        UTButton {
                            Layout.fillWidth: true
                            text: "复制 Key"
                            buttonType: UTButton.ButtonType.Tonal
                            font.pixelSize: 10
                            onClicked: {
                                root.toast && root.toast.showToast("已复制: " + model.colorKey, UTToast.ToastType.Success, 1500);
                            }
                        }
                    }

                    TapHandler {
                        onTapped: {
                            root.toast && root.toast.showToast("Key: " + model.colorKey, UTToast.ToastType.Info, 2000);
                        }
                    }
                }
            }
        }

        // 主题切换按钮
        RowLayout {
            spacing: 12

            UTButton {
                text: "Light"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: window.setTheme("light")
            }
            UTButton {
                text: "Dark"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: window.setTheme("dark")
            }
            UTButton {
                text: "Warm"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: window.setTheme("warm")
            }
            UTButton {
                text: "Forest"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: window.setTheme("forest")
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }

    // 颜色数据模型
    ListModel {
        id: colorGridModel

        Component.onCompleted: {
            let keys = root.colorKeys;
            for (let i = 0; i < keys.length; i++) {
                let key = keys[i];
                let value = getThemeColor(key);
                append({
                    "colorKey": key,
                    "colorName": formatKeyName(key),
                    "colorValue": value
                });
            }
        }

        function getThemeColor(key) {
            let parts = key.split('.');
            let value = theme;
            for (let part of parts) {
                if (value && value[part] !== undefined) {
                    value = value[part];
                } else {
                    return "#000000";
                }
            }
            return value.toString();
        }

        function formatKeyName(key) {
            return key.replace(/\./g, ' / ').replace(/([A-Z])/g, ' $1').replace(/^./, function (str) {
                return str.toUpperCase();
            });
        }
    }
}
