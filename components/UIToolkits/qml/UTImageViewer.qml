/*
 * @file UTImageViewer.qml
 * @brief 图片查看器 — 支持拖拽平移、滚轮缩放、适应窗口
 */

// ========================================================================
// ========================================================================

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs

Item {
    id: root

    // ── Theme ────────────────────────────────────────────────────

    // ── Public API ────────────────────────────────────────────────
    /// 图片源 URL
    property url source: ""
    /// 当前缩放级别（只读；通过 setZoom() 修改）
    readonly property real zoomLevel: _realZoom
    /// 适应窗口的缩放级别 (只读)
    readonly property real fitZoom: _fitZoom
    /// 实时图片尺寸 (只读)
    readonly property size imageSourceSize: _image.sourceSize
    /// 图片在预览组件内的实际绘制位置
    readonly property point imagePosition: Qt.point(_image.x, _image.y)
    /// 图片当前实际缩放比例
    readonly property real imageZoom: _realZoom
    /// 是否有图片加载成功 (只读)
    readonly property bool hasImage: _image.status === Image.Ready
    /// 是否正在加载 (只读)
    readonly property bool isLoading: _image.status === Image.Loading
    /// 是否加载失败 (只读)
    readonly property bool isError: _image.status === Image.Error
    /// 选择图片时的文件过滤器
    property string fileFilters: "Image files (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff)"
    /// 是否允许拖拽平移
    property bool panEnabled: true
    /// 是否允许滚轮缩放
    property bool zoomEnabled: true
    /// 是否允许双击适应窗口
    property bool doubleClickFitEnabled: true
    /// 是否缓存图片 (image:// 提供者必须为 true)
    property bool cache: true
    /// 图源更新后保持当前缩放与平移；适合参数联动的同一图像视图。
    property bool preserveViewOnSourceChange: false

    // ── Internal state ───────────────────────────────────────────
    property real _fitZoom: 1.0
    property real _panX: 0
    property real _panY: 0
    property real _realZoom: 1.0
    property bool _needsInitialFit: true
    property int _fitRequestRevision: 0
    property bool _fitAfterNextSourceChange: false
    property bool _fitReplacementActive: false
    property bool _awaitingReplacementReady: false
    property real _fittedSourceWidth: 0
    property real _fittedSourceHeight: 0

    // ── Theme-aware colors ───────────────────────────────────────
    property color viewportBg: UTTheme.isDark !== undefined && UTTheme.isDark ? "#1A1A1A" : "#E8E8E8"
    /// 预览区域边框颜色
    property color borderColor: borderNormal
    /// 预览区域边框宽度
    property real borderWidth: 1
    /// 预览区域圆角半径
    property real cornerRadius: UTTheme.radius !== undefined ? UTTheme.radius : 8
    /// 兼容旧调用；新代码请使用 borderColor
    property color borderNormal: UTTheme.borderColor
    property color textColor: UTTheme.primaryTextColor
    property color textDimColor: UTTheme.secondaryTextColor

    /// 无图片时的占位提示
    property string placeholder: ""

    implicitWidth: 400
    implicitHeight: 300

    function requestInitialFit() {
        if (!_needsInitialFit || _awaitingReplacementReady || _image.status !== Image.Ready ||
                _image.sourceSize.width <= 0 || _image.sourceSize.height <= 0
                || viewport.width <= 0 || viewport.height <= 0)
            return;
        const requestRevision = root._fitRequestRevision;
        Qt.callLater(function() {
            if (requestRevision !== root._fitRequestRevision || !_needsInitialFit || _awaitingReplacementReady ||
                    _image.status !== Image.Ready || _image.sourceSize.width <= 0 || _image.sourceSize.height <= 0
                    || viewport.width <= 0 || viewport.height <= 0)
                return;
            root.fitToViewport();
            root._needsInitialFit = false;
        });
    }

    onSourceChanged: {
        // Invalidate a delayed fit that still belongs to the preceding image.
        // Image.retainWhileLoading may otherwise let that callback observe the
        // old Ready image after a new URL has already been assigned.
        ++_fitRequestRevision;
        const fitReplacement = _fitAfterNextSourceChange;
        _fitAfterNextSourceChange = false;
        _fitReplacementActive = fitReplacement;
        // Correction previews may refresh their image URL while the user is
        // inspecting a particular area.  Retain the view after the first
        // successful fit so an update behaves like Python's preserve_view.
        if (!preserveViewOnSourceChange || _needsInitialFit || fitReplacement)
            _needsInitialFit = true;
        // retainWhileLoading deliberately keeps the old image Ready while the
        // provider resolves the replacement URL.  Ignore that retained frame
        // until Image reports the new source as Ready.
        if (fitReplacement)
            _awaitingReplacementReady = true;
    }

    // ── 视图容器 ────────────────────────────────────────────────
    Rectangle {
        id: viewport
        anchors.fill: parent
        color: root.viewportBg
        border.color: root.borderColor
        border.width: root.borderWidth
        radius: root.cornerRadius
        clip: true

        onWidthChanged: root.requestInitialFit()
        onHeightChanged: root.requestInitialFit()

        // 无图片时的占位提示
        Item {
            id: placeholderItem
            anchors.centerIn: parent
            visible: !root.hasImage && !root.isLoading

            Column {
                anchors.centerIn: parent
                spacing: 12

                Label {
                    text: "🖼"
                    font.family: UTTheme.fontFamily
                    font.pixelSize: 48
                    anchors.horizontalCenter: parent.horizontalCenter
                    opacity: 0.3
                }

                Label {
                    text: root.placeholder
                    font.family: UTTheme.fontFamily
                    font.pixelSize: 14
                    color: root.textDimColor
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // 图片
        Image {
            id: _image
            source: root.source
            asynchronous: true
            cache: root.cache
            retainWhileLoading: true
            smooth: true
            fillMode: Image.PreserveAspectFit

            width: sourceSize.width > 0 ? sourceSize.width * root._realZoom : 0
            height: sourceSize.height > 0 ? sourceSize.height * root._realZoom : 0
            x: sourceSize.width > 0 ? viewport.width / 2 - width / 2 + root._panX : viewport.width / 2
            y: sourceSize.height > 0 ? viewport.height / 2 - height / 2 + root._panY : viewport.height / 2

            onSourceSizeChanged: {
                // With retainWhileLoading, Qt may report Ready before replacing
                // sourceSize. A fit performed in that interval used the old
                // dimensions and must not suppress the fit for the actual image.
                if (root._fitReplacementActive &&
                        sourceSize.width > 0 && sourceSize.height > 0 &&
                        root._fittedSourceWidth > 0 && root._fittedSourceHeight > 0 &&
                        (sourceSize.width !== root._fittedSourceWidth ||
                         sourceSize.height !== root._fittedSourceHeight)) {
                    root._awaitingReplacementReady = false;
                    root._needsInitialFit = true;
                }
                root.requestInitialFit();
            }

            onStatusChanged: {
                if (status === Image.Ready) {
                    root._awaitingReplacementReady = false;
                    root.requestInitialFit();
                } else if (status === Image.Error) {
                    root._awaitingReplacementReady = false;
                    UTLogger.warn("Image load failed", "UIToolkits.ImageViewer");
                }
            }

            // 加载中指示器
            BusyIndicator {
                anchors.centerIn: parent
                running: root.isLoading
                visible: root.isLoading
            }
        }

        // ── 鼠标/触控交互 ───────────────────────────────────────
        MouseArea {
            id: mouseArea
            anchors.fill: parent

            preventStealing: true
            scrollGestureEnabled: false
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            property real _dragStartX: 0
            property real _dragStartY: 0
            property bool _isDragging: false

            // 滚轮缩放
            onWheel: function (wheel) {
                if (placeholderItem.visible || !root.zoomEnabled)
                    return;
                var oldZoom = root._realZoom;
                var delta = wheel.angleDelta.y > 0 ? 1.1 : 1 / 1.1;
                var newZoom = Math.max(0.05, Math.min(20.0, oldZoom * delta));
                // 以鼠标位置为中心缩放
                var viewX = wheel.x - viewport.width / 2;
                var viewY = wheel.y - viewport.height / 2;
                root._panX = viewX - (viewX - root._panX) * newZoom / oldZoom;
                root._panY = viewY - (viewY - root._panY) * newZoom / oldZoom;
                root._realZoom = newZoom;
            }

            onPressed: function (mouse) {
                if (placeholderItem.visible)
                    return;
                _dragStartX = root._panX - mouse.x + viewport.width / 2;
                _dragStartY = root._panY - mouse.y + viewport.height / 2;
                _isDragging = false;
            }

            onPositionChanged: function (mouse) {
                if (!pressed || placeholderItem.visible || !root.panEnabled)
                    return;
                _isDragging = true;
                root._panX = mouse.x - viewport.width / 2 + _dragStartX;
                root._panY = mouse.y - viewport.height / 2 + _dragStartY;
            }

            onReleased: function (mouse) {
                _isDragging = false;
            }

            onDoubleClicked: function (mouse) {
                if (placeholderItem.visible || !root.doubleClickFitEnabled)
                    return;
                root.fitToViewport();
            }

            cursorShape: {
                if (placeholderItem.visible)
                    return Qt.ArrowCursor;
                if (pressed && _isDragging)
                    return Qt.ClosedHandCursor;
                return Qt.OpenHandCursor;
            }
        }
    }

    // ── 底部悬浮信息栏 ──────────────────────────────────────────
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        color: Qt.rgba(0, 0, 0, 0.45)
        radius: 4
        width: imageInfo.implicitWidth + 16
        height: imageInfo.implicitHeight + 8
        visible: root.hasImage
        z: 1

        Label {
            id: imageInfo
            font.family: UTTheme.fontFamily
            anchors.centerIn: parent
            text: root.hasImage ? ("%1% | %2 × %3").arg(Math.round(root._realZoom * 100)).arg(root.imageSourceSize.width).arg(root.imageSourceSize.height) : ""
            font.pixelSize: 11
            color: "#d8dce2"
        }
    }

    // ── Public Functions ─────────────────────────────────────────

    /// 从本地文件路径加载图片
    function loadFromFile(filePath) {
        root.source = "file:///" + filePath;
    }

    /// 从 URL 加载图片
    function loadFromUrl(url) {
        root.source = url;
    }

    /// 适应窗口: 将图片缩放至刚好填充视图
    function fitToViewport() {
        if (_image.sourceSize.width <= 0 || _image.sourceSize.height <= 0)
            return;
        var scaleW = viewport.width / _image.sourceSize.width;
        var scaleH = viewport.height / _image.sourceSize.height;
        root._fitZoom = Math.min(scaleW, scaleH, 1.0);
        root._realZoom = root._fitZoom;
        root._panX = 0;
        root._panY = 0;
        root._fittedSourceWidth = _image.sourceSize.width;
        root._fittedSourceHeight = _image.sourceSize.height;
    }

    /// 清除图片
    function clear() {
        root.source = "";
        root._realZoom = 1.0;
        root._fitZoom = 1.0;
        root._panX = 0;
        root._panY = 0;
    }

    /// 重置缩放与平移
    function resetView() {
        // resetView() is also used immediately before an asynchronous source
        // replacement. Keep the fit request armed after fitting the current
        // image, otherwise preserveViewOnSourceChange would carry the old
        // image's scale into the new image and require a manual double-click.
        ++root._fitRequestRevision;
        root._fitAfterNextSourceChange = root.preserveViewOnSourceChange;
        root._awaitingReplacementReady = false;
        root._needsInitialFit = !root.preserveViewOnSourceChange;
        root._panX = 0;
        root._panY = 0;
        if (_image.sourceSize.width > 0 && _image.sourceSize.height > 0) {
            root.fitToViewport();
            return;
        }
        root._realZoom = 1.0;
        root._fitZoom = 1.0;
    }

    function setZoom(value) {
        if (!hasImage)
            return;
        root._realZoom = Math.max(0.05, Math.min(20.0, Number(value)));
    }

    /// 将视图坐标转换为图片坐标
    function viewportToImage(vx, vy) {
        return Qt.point((vx - _image.x) / root._realZoom, (vy - _image.y) / root._realZoom);
    }
}
