/*
 * @file UTImage.qml
 * @brief UIToolkits 的 UTImage.qml 组件。
 */

import QtQuick
import QtQuick.Effects

Item {
    id: root

    // Keep this as a string so a raw Windows path is not converted to a
    // one-letter URL scheme before normalizedSource() can handle it.
    property string source: ""
    property int imgSize: 24
    readonly property int status: rawImage.status
    readonly property bool loading: rawImage.status === Image.Loading
    readonly property bool hasError: rawImage.status === Image.Error

    implicitWidth: imgSize
    implicitHeight: imgSize

    signal loadFailed(string source)

    // QML converts a raw Windows path such as C:/image.png to a URL with
    // scheme "c".  QQuickImage then reports "Protocol c is unknown".  Keep
    // the public property as url, but normalize drive-letter paths before
    // forwarding them to Image.
    function normalizedSource(value) {
        let path = value === undefined || value === null ? "" : value.toString();
        // A bare drive letter is an incomplete path, not a URL.  Treat it as
        // an empty source instead of passing scheme "c" to QQuickImage.
        if (/^[A-Za-z]:?$/.test(path))
            return "";
        if (/^[A-Za-z]:[\\/]/.test(path))
            return "file:///" + path.replace(/\\/g, "/");
        if (path.startsWith("\\\\"))
            return "file:" + path.replace(/\\/g, "/");
        return path;
    }

    Image {
        id: rawImage
        anchors.fill: parent
        source: root.normalizedSource(root.source)
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
