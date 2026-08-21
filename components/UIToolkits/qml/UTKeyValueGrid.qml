/*
 * @file UTKeyValueGrid.qml
 * @brief UIToolkits 的 UTKeyValueGrid.qml 组件。
 */

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property var rows: []
    property int rowSpacing: 8
    property int columnSpacing: 12
    property color labelColor: UTTheme.secondaryTextColor
    property color valueColor: UTTheme.primaryTextColor

    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content
        width: parent.width
        spacing: root.rowSpacing

        Repeater {
            model: root.rows

            delegate: RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: root.columnSpacing

                UTLabel {
                    text: modelData.label || ""
                    color: root.labelColor
                }
                UTLabel {
                    Layout.fillWidth: true
                    text: modelData.value || "-"
                    color: root.valueColor
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
        }
    }
}
