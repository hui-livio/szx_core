/*
 * @file UTTreeView.qml
 * @brief UIToolkits 的 UTTreeView.qml 组件。
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

TreeView {
    id: root

    // ── Properties ───────────────────────────────────────────────
    property string title: "Tree View"

    /// 由 delegate 中 TapHandler 设置的当前选中行数据，供外部读取
    property var currentItemData: null
    property bool keyboardNavigationEnabled: true
    /// 允许业务页面接管 Enter/Return；Space 仍激活当前树节点。
    property bool enterKeyPassthrough: false
    property bool wrapNavigation: false
    property int pageStep: Math.max(1, Math.floor(root.height / 40))
    property var keyboardItemDataProvider: null
    /// 可选的键盘行过滤器。返回 true 的可见行才参与方向键导航。
    property var keyboardNavigationRowFilter: null
    /// 启用后，方向键选中目标行立即复用 itemClicked 激活语义。
    property bool activateOnKeyboardNavigation: false
    readonly property int selectedKeyboardRow: root.currentRow

    implicitWidth: 200
    clip: true
    // 不使用 reuseItems：DelegateChooser 多类型 delegate + Behavior color 动画
    // + TapHandler 组合下，回收 delegate 会导致闪烁和误点击（Qt 6.5 已知问题）
    reuseItems: false

    /// 允许 Tab 键聚焦
    activeFocusOnTab: true

    Accessible.name: root.title
    Accessible.role: Accessible.List

    // ====================================================================
    // 列宽
    // ====================================================================
    columnWidthProvider: function (column) {
        return width;
    }

    // ====================================================================
    // 键盘导航 & 选中模型
    // ItemSelectionModel 维护当前选择；Keys.BeforeItem 在 TreeView
    // 内部处理前接管导航键，确保 Enter/Space 与鼠标激活语义一致。
    // ====================================================================
    selectionModel: ItemSelectionModel {
        id: selModel
        model: root.model
    }

    // ── Signals ───────────────────────────────────────────────────
    /// 当 delegate 中用户点击某行时发出，携带该行的自定义数据对象
    /// 约定：delegate 的 TapHandler 负责收集 model 数据并调用此信号
    /// 示例：
    ///   TapHandler { onTapped: { let d = { id: model.itemId, name: model.itemName };
    ///                            tree.currentItemData = d; tree.itemClicked(d); } }
    signal itemClicked(var itemData)
    signal activated(int row, var itemData)
    signal confirmationRequested()

    function collapseAll() {
        collapseRecursively();
    }

    function expandAll() {
        expandRecursively();
    }

    function normalizedRow(row) {
        if (root.rows <= 0)
            return -1;
        if (root.wrapNavigation)
            return (row % root.rows + root.rows) % root.rows;
        return Math.max(0, Math.min(root.rows - 1, row));
    }

    function itemDataForRow(row) {
        const index = root.index(row, 0);
        if (root.keyboardItemDataProvider)
            return root.keyboardItemDataProvider(row, index);
        return { "row": row };
    }

    function selectRow(row) {
        const nextRow = root.normalizedRow(row);
        if (nextRow < 0)
            return false;
        const index = root.index(nextRow, 0);
        if (!index.valid)
            return false;
        root.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect);
        root.currentItemData = root.itemDataForRow(nextRow);
        root.positionViewAtRow(nextRow, TreeView.Contain);
        return true;
    }

    function nextKeyboardNavigationRow(firstRow, delta, clampAtBoundary) {
        if (root.rows <= 0 || delta === 0)
            return -1;

        let direction = delta < 0 ? -1 : 1;
        let row = firstRow;
        if (!root.wrapNavigation && clampAtBoundary) {
            if (row < 0) {
                row = 0;
                direction = 1;
            } else if (row >= root.rows) {
                row = root.rows - 1;
                direction = -1;
            }
        }
        for (let visited = 0; visited < root.rows; ++visited) {
            if (root.wrapNavigation)
                row = (row % root.rows + root.rows) % root.rows;
            else if (row < 0 || row >= root.rows)
                return -1;

            const itemData = root.itemDataForRow(row);
            if (!root.keyboardNavigationRowFilter || root.keyboardNavigationRowFilter(row, itemData))
                return row;
            row += direction;
        }
        return -1;
    }

    function moveCurrentBy(delta, clampAtBoundary) {
        const baseRow = root.selectedKeyboardRow >= 0 ? root.selectedKeyboardRow : (delta >= 0 ? -1 : root.rows);
        const row = root.keyboardNavigationRowFilter ? root.nextKeyboardNavigationRow(baseRow + delta, delta, clampAtBoundary === true) : baseRow + delta;
        const selected = row >= 0 && root.selectRow(row);
        if (selected && root.activateOnKeyboardNavigation)
            root.activateCurrent();
        return selected;
    }

    function moveToFirst() {
        const row = root.keyboardNavigationRowFilter ? root.nextKeyboardNavigationRow(0, 1) : 0;
        const selected = row >= 0 && root.selectRow(row);
        if (selected && root.activateOnKeyboardNavigation)
            root.activateCurrent();
        return selected;
    }

    function moveToLast() {
        const row = root.keyboardNavigationRowFilter ? root.nextKeyboardNavigationRow(root.rows - 1, -1) : root.rows - 1;
        const selected = row >= 0 && root.selectRow(row);
        if (selected && root.activateOnKeyboardNavigation)
            root.activateCurrent();
        return selected;
    }

    function activateCurrent() {
        const row = root.selectedKeyboardRow;
        if (row < 0 || row >= root.rows)
            return false;
        const itemData = root.itemDataForRow(row);
        root.currentItemData = itemData;
        root.activated(row, itemData);
        root.itemClicked(itemData);
        return true;
    }

    Keys.priority: Keys.BeforeItem
    Keys.onPressed: function(event) {
        if (!root.keyboardNavigationEnabled)
            return;
        if (event.key === Qt.Key_Up) {
            event.accepted = root.moveCurrentBy(-1) || (root.keyboardNavigationRowFilter && root.rows > 0);
        } else if (event.key === Qt.Key_Down) {
            event.accepted = root.moveCurrentBy(1) || (root.keyboardNavigationRowFilter && root.rows > 0);
        } else if (event.key === Qt.Key_PageUp) {
            event.accepted = root.moveCurrentBy(-root.pageStep, true) || (root.keyboardNavigationRowFilter && root.rows > 0);
        } else if (event.key === Qt.Key_PageDown) {
            event.accepted = root.moveCurrentBy(root.pageStep, true) || (root.keyboardNavigationRowFilter && root.rows > 0);
        } else if (event.key === Qt.Key_Home) {
            event.accepted = root.moveToFirst() || (root.keyboardNavigationRowFilter && root.rows > 0);
        } else if (event.key === Qt.Key_End) {
            event.accepted = root.moveToLast() || (root.keyboardNavigationRowFilter && root.rows > 0);
        } else if (event.key === Qt.Key_Left) {
            if (root.selectedKeyboardRow >= 0 && root.isExpanded(root.selectedKeyboardRow)) {
                root.collapseRecursively(root.selectedKeyboardRow);
                event.accepted = true;
            }
        } else if (event.key === Qt.Key_Right) {
            if (root.selectedKeyboardRow >= 0 && !root.isExpanded(root.selectedKeyboardRow)) {
                root.expandRecursively(root.selectedKeyboardRow);
                event.accepted = true;
            }
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (root.enterKeyPassthrough) {
                // TreeView may consume an unaccepted Enter after Keys.BeforeItem.
                // Route the business action explicitly and stop item activation.
                event.accepted = true;
                root.confirmationRequested();
            } else {
                event.accepted = root.activateCurrent();
            }
        } else if (event.key === Qt.Key_Space) {
            event.accepted = root.activateCurrent();
        }
    }

    Component.onCompleted: {
        expandRecursively();
    }
}
