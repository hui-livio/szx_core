/*
 * @file DemoFeedback.qml
 * @brief UIToolkits 展示应用的 DemoFeedback.qml 页面。
 */

import QtQuick
import QtQuick.Layouts
import UIToolkits

UTFlickable {
    id: root

    property var toast: null
    property real loadingOverlayOpacity: 0.42
    property bool loadingHighlighted: true
    property int loadingIndicatorStyle: UTLoading.FluentRing
    property UTLoading activeLoading: null

    readonly property var theme: UTTheme.theme

    contentWidth: contentCol.width
    contentHeight: contentCol.implicitHeight

    function showLoading(loading) {
        if (activeLoading && activeLoading !== loading)
            activeLoading.active = false
        activeLoading = loading
        loading.active = true
        loadingPreviewTimer.restart()
    }

    ColumnLayout {
        id: contentCol
        width: root.width - 14
        spacing: 24

        UTLabel {
            text: "Feedback"
            textRole: UTLabel.TextRole.H2
            color: root.theme.primaryTextColor
        }

        UTLabel {
            text: "Toast Notifications"
            textRole: UTLabel.TextRole.H3
            color: root.theme.primaryTextColor
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UTButton {
                text: "Info Toast"
                buttonType: UTButton.ButtonType.Filled
                onClicked: root.toast && root.toast.showToast("Saved successfully", UTToast.ToastType.Info, 1800)
            }
            UTButton {
                text: "Warn Toast"
                buttonType: UTButton.ButtonType.Outlined
                onClicked: root.toast && root.toast.showToast("Network is unstable", UTToast.ToastType.Warning, 2000)
            }
            UTButton {
                text: "Error Toast"
                buttonType: UTButton.ButtonType.Tonal
                onClicked: root.toast && root.toast.showToast("Operation failed", UTToast.ToastType.Error, 2000)
            }
            UTButton {
                text: "Success Toast"
                buttonType: UTButton.ButtonType.Text
                onClicked: root.toast && root.toast.showToast("Done!", UTToast.ToastType.Success, 1500)
            }
        }

        UTLabel {
            text: qsTr("Loading")
            textRole: UTLabel.TextRole.H3
            color: root.theme.primaryTextColor
        }

        UTCard {
            Layout.fillWidth: true
            Layout.preferredHeight: loadingDemoLayout.implicitHeight + 32

            ColumnLayout {
                id: loadingDemoLayout
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true

                    UTButton {
                        text: qsTr("Basic Modal")
                        buttonType: UTButton.ButtonType.Filled
                        onClicked: root.showLoading(loadingOverlay)
                    }

                    UTButton {
                        text: qsTr("With Progress")
                        buttonType: UTButton.ButtonType.Outlined
                        onClicked: root.showLoading(progressLoading)
                    }

                    UTLabel {
                        text: qsTr("Modal preview for 2 seconds")
                        color: root.theme.secondaryTextColor
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    UTLabel {
                        text: qsTr("Opacity")
                        color: root.theme.secondaryTextColor
                    }

                    UTSlider {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: root.loadingOverlayOpacity * 100
                        showValue: true
                        valueSuffix: "%"
                        onMoved: root.loadingOverlayOpacity = value / 100
                    }

                    UTLabel {
                        text: Math.round(root.loadingOverlayOpacity * 100) + "%"
                        color: root.theme.primaryTextColor
                    }
                }

                UTSwitch {
                    text: qsTr("Highlight border")
                    checked: root.loadingHighlighted
                    onToggled: root.loadingHighlighted = checked
                }

                UTLabel {
                    text: qsTr("Use contentComponent or nested content to add progress and extra instructions.")
                    color: root.theme.secondaryTextColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true

                    UTLabel {
                        text: qsTr("Indicator style")
                        color: root.theme.secondaryTextColor
                    }

                    UTCombobox {
                        Layout.preferredWidth: 180
                        model: [qsTr("Fluent ring"), qsTr("Windows dots")]
                        currentIndex: root.loadingIndicatorStyle === UTLoading.FluentRing ? 0 : 1
                        onActivated: function (index) {
                            root.loadingIndicatorStyle = index === 0
                                    ? UTLoading.FluentRing : UTLoading.WindowsDots
                        }
                    }
                }
            }

            UTLoading {
                id: loadingOverlay
                parent: root
                active: false
                text: qsTr("Loading preview…")
                panelOpacity: root.loadingOverlayOpacity
                highlighted: root.loadingHighlighted
                indicatorStyle: root.loadingIndicatorStyle
            }

            UTLoading {
                id: progressLoading
                parent: root
                active: false
                text: qsTr("Synchronizing template data…")
                panelOpacity: root.loadingOverlayOpacity
                highlighted: root.loadingHighlighted
                indicatorStyle: root.loadingIndicatorStyle

                contentComponent: Component {
                    Column {
                        spacing: 8
                        width: 220

                        UTProgressBar {
                            width: parent.width
                            from: 0
                            to: 100
                            value: 68
                        }

                        UTLabel {
                            text: qsTr("Downloading model configuration (68%)")
                            color: UTTheme.secondaryTextColor
                            horizontalAlignment: Text.AlignHCenter
                            width: parent.width
                        }
                    }
                }
            }
        }

        UTLabel {
            text: "Context Menu"
            textRole: UTLabel.TextRole.H3
            color: root.theme.primaryTextColor
        }

        UTButton {
            id: menuButton
            text: "Open Menu"
            buttonType: UTButton.ButtonType.Outlined
            onClicked: actionsMenu.popup(menuButton, 0, menuButton.height + 4)
        }

        UTMenu {
            id: actionsMenu
            actionsModel: [
                {
                    text: "Copy",
                    enabled: true
                },
                {
                    text: "Rename",
                    enabled: true
                },
                {
                    text: "Delete",
                    enabled: true
                }
            ]
            onActionTriggered: function (index, text) {
                if (root.toast)
                    root.toast.showToast("Menu: " + text, UTToast.ToastType.Success, 1500)
            }
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }

    Timer {
        id: loadingPreviewTimer
        interval: 2000
        repeat: false
        onTriggered: {
            if (root.activeLoading)
                root.activeLoading.active = false
            root.activeLoading = null
        }
    }

}
