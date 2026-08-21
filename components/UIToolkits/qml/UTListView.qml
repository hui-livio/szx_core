/*
 * @file UTListView.qml
 * @brief UIToolkits 的 UTListView.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

ListView {
    id: root

    // 无障碍
    property string accName: ""
    property int accRole: Accessible.List
    property bool keyboardNavigationEnabled: true
    property bool wrapNavigation: false
    property int pageStep: Math.max(1, Math.floor(root.height / Math.max(1, root.currentItem ? root.currentItem.height : 32)))

    Accessible.name: accName
    Accessible.role: accRole

    // 信号：Enter/Space 激活当前项
    signal activated(int index)

    activeFocusOnTab: true
    keyNavigationEnabled: keyboardNavigationEnabled
    highlightFollowsCurrentItem: true

    function normalizedIndex(index) {
        if (root.count <= 0)
            return -1;
        if (root.wrapNavigation)
            return (index % root.count + root.count) % root.count;
        return Math.max(0, Math.min(root.count - 1, index));
    }

    function selectIndex(index) {
        const nextIndex = root.normalizedIndex(index);
        if (nextIndex < 0)
            return false;
        root.currentIndex = nextIndex;
        root.positionViewAtIndex(nextIndex, ListView.Contain);
        return true;
    }

    function moveCurrentBy(delta) {
        const baseIndex = root.currentIndex >= 0 ? root.currentIndex : (delta >= 0 ? -1 : root.count);
        return root.selectIndex(baseIndex + delta);
    }

    function moveToFirst() {
        return root.selectIndex(0);
    }

    function moveToLast() {
        return root.selectIndex(root.count - 1);
    }

    function activateCurrent() {
        if (root.currentIndex >= 0 && root.currentIndex < root.count) {
            root.activated(root.currentIndex);
            return true;
        }
        return false;
    }

    Keys.onPressed: function(event) {
        if (!root.keyboardNavigationEnabled)
            return;
        if (event.key === Qt.Key_Up) {
            event.accepted = root.moveCurrentBy(-1);
        } else if (event.key === Qt.Key_Down) {
            event.accepted = root.moveCurrentBy(1);
        } else if (event.key === Qt.Key_PageUp) {
            event.accepted = root.moveCurrentBy(-root.pageStep);
        } else if (event.key === Qt.Key_PageDown) {
            event.accepted = root.moveCurrentBy(root.pageStep);
        } else if (event.key === Qt.Key_Home) {
            event.accepted = root.moveToFirst();
        } else if (event.key === Qt.Key_End) {
            event.accepted = root.moveToLast();
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            event.accepted = root.activateCurrent();
        }
    }

    // ── 当前项高亮 ───────────────────────────────────────────────
    highlight: Rectangle {
        color: UTTheme.primaryColor ? Qt.rgba(UTTheme.primaryColor.r, UTTheme.primaryColor.g, UTTheme.primaryColor.b, 0.12) : "#1a3a6df0"
        radius: UTTheme.radius ? UTTheme.radius * 0.5 : 4
        Behavior on y {
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    highlightMoveDuration: 120
    highlightResizeDuration: 0
}
