/*
 * @file UTImage.qml
 * @brief UIToolkits 的 UTImage.qml 组件。
 */

import QtQuick
import QtQuick.Effects

Item {
    id: root

    property url source: ""
    property int imgSize: 24
    readonly property int status: rawImage.status
    readonly property bool loading: rawImage.status === Image.Loading
    readonly property bool hasError: rawImage.status === Image.Error

    implicitWidth: imgSize
    implicitHeight: imgSize

    signal loadFailed(url source)

    Image {
        id: rawImage
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(root.imgSize * 2, root.imgSize * 2)
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        onStatusChanged: {
            if (status === Image.Error)
                root.loadFailed(root.source);
        }
    }
}
