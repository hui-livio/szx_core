import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

Rectangle {
    id: root

    required property string pageTitle
    required property string description

    color: "transparent"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12

        UTLabel {
            Layout.alignment: Qt.AlignHCenter
            text: root.pageTitle
            textRole: UTLabel.TextRole.H2
        }

        UTLabel {
            Layout.alignment: Qt.AlignHCenter
            text: root.description
            color: UTTheme.secondaryTextColor
        }
    }
}
