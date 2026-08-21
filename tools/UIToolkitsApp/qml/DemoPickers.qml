/*
 * @file DemoPickers.qml
 * @brief UIToolkits 展示应用的 DemoPickers.qml 页面。
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
        spacing: 24

        UTLabel {
            text: "Date / Time Pickers"
            textRole: UTLabel.TextRole.H2
            color: theme.primaryTextColor
        }

        UTDatePicker {
            Layout.fillWidth: true
            dateValue: "2026-06-03"
            onValueChanged: function (value) {
                root.toast && root.toast.showToast("Date: " + value, UTToast.ToastType.Info, 1200);
            }
        }

        UTTimePicker {
            Layout.fillWidth: true
            showSeconds: true
            timeValue: "14:30:15"
        }

        UTDateTimePicker {
            Layout.fillWidth: true
            showSeconds: true
            dateTimeValue: "2026-06-03 14:30:15"
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: 24
        }
    }
}
