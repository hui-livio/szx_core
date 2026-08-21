/*
 * @file UTMenu.qml
 * @brief UIToolkits 的 UTMenu.qml 组件。
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Menu {
    id: root

    /// 菜单项模型。分支项通过 children 声明任意层级的子菜单。
    property var actionsModel: []

    /// actionsModel 原地更新后递增该值，或直接调用 refresh() 刷新菜单。
    property int modelRevision: 0

    /// 子菜单内部使用的完整索引路径前缀。
    property list<int> actionPathPrefix

    property list<QtObject> generatedObjects
    property bool componentReady: false
    property string fontFamily: UTTheme.fontFamily
    property int fontPixelSize: 13
    property int fontWeight: Font.Normal
    readonly property int generatedActionKind: 1
    readonly property int generatedMenuKind: 2
    readonly property int generatedItemKind: 3

    /// 兼容原有一级菜单 API；多级菜单中的 index 是叶子在直接父菜单中的索引。
    signal actionTriggered(int index, string text)

    /// 层级安全的动作信号，返回完整 action 对象和从根菜单开始的索引路径。
    signal actionSelected(var action, var indexPath)

    padding: 6
    margins: 4
    overlap: 2
    cascade: true
    modal: false
    focus: true

    delegate: menuItemComponent

    background: Rectangle {
        implicitWidth: 160
        radius: UTTheme.radius || 8
        color: UTTheme.surfaceColor
        border.width: 1
        border.color: UTTheme.borderColor
    }

    Component {
        id: menuItemComponent

        MenuItem {
            id: menuItem
            objectName: "menuItem"

            implicitWidth: Math.max(160, implicitContentWidth + leftPadding + rightPadding)
            implicitHeight: 34
            leftPadding: 34
            rightPadding: 30
            font.family: root.fontFamily
            font.pixelSize: root.fontPixelSize
            font.weight: root.fontWeight

            contentItem: Text {
                text: menuItem.text
                color: menuItem.enabled ? UTTheme.primaryTextColor : UTTheme.secondaryTextColor
                font.family: root.fontFamily
                font.pixelSize: root.fontPixelSize
                font.weight: menuItem.highlighted ? Font.DemiBold : root.fontWeight
                textFormat: Text.PlainText
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            indicator: Text {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                visible: menuItem.checkable
                text: menuItem.checked ? "✓" : ""
                color: menuItem.enabled ? UTTheme.primaryColor : UTTheme.secondaryTextColor
                font.family: root.fontFamily
                font.pixelSize: root.fontPixelSize
                font.weight: Font.DemiBold
                textFormat: Text.PlainText
            }

            arrow: Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                visible: menuItem.subMenu !== null
                text: menuItem.mirrored ? "‹" : "›"
                color: menuItem.enabled ? UTTheme.primaryTextColor : UTTheme.secondaryTextColor
                font.family: root.fontFamily
                font.pixelSize: root.fontPixelSize + 4
                textFormat: Text.PlainText
            }

            background: Rectangle {
                radius: Math.max(2, (UTTheme.radius || 8) - 2)
                color: menuItem.highlighted
                       ? Qt.rgba(UTTheme.primaryColor.r, UTTheme.primaryColor.g, UTTheme.primaryColor.b, 0.10)
                       : "transparent"
            }
        }
    }

    Component {
        id: actionComponent

        Action {
            property int generatedKind: root.generatedActionKind
            property int sourceIndex: -1
            property var sourceAction: null
            property list<int> sourcePath

            onTriggered: {
                root.updateActionChecked(sourcePath, checked);

                root.triggerAction(sourceIndex, sourceAction, sourcePath);
            }
        }
    }

    Component {
        id: separatorComponent

        MenuSeparator {
            property int generatedKind: root.generatedItemKind
            implicitHeight: 9
            topPadding: 4
            bottomPadding: 4

            contentItem: Rectangle {
                implicitHeight: 1
                color: UTTheme.borderColor
            }
        }
    }

    Component {
        id: submenuComponent

        Menu {
            id: submenu

            property int generatedKind: root.generatedMenuKind
            property var actionsModel: []
            property list<int> actionPathPrefix
            property list<QtObject> generatedObjects

            padding: root.padding
            margins: root.margins
            overlap: root.overlap
            cascade: root.cascade
            modal: false
            focus: true
            delegate: menuItemComponent

            background: Rectangle {
                implicitWidth: 160
                radius: UTTheme.radius || 8
                color: UTTheme.surfaceColor
                border.width: 1
                border.color: UTTheme.borderColor
            }

            Component.onCompleted: Qt.callLater(function () {
                root.populateMenu(submenu, submenu.actionsModel, submenu.actionPathPrefix);
            })
        }
    }

    onActionsModelChanged: {
        if (root.componentReady)
            root.rebuildMenu();
    }

    onModelRevisionChanged: {
        if (root.componentReady)
            root.rebuildMenu();
    }

    Component.onCompleted: {
        root.componentReady = true;
        root.rebuildMenu();
    }

    function refresh() {
        root.rebuildMenu();
    }

    function actionChildren(action) {
        if (!action)
            return [];

        return action.children || [];
    }

    function actionAtPath(indexPath) {
        let actions = root.actionsModel || [];
        let action = null;
        for (let depth = 0; depth < indexPath.length; ++depth) {
            const index = indexPath[depth];
            if (index < 0 || index >= actions.length)
                return null;

            action = actions[index];
            actions = root.actionChildren(action);
        }
        return action;
    }

    function updateActionChecked(indexPath, checked) {
        const action = root.actionAtPath(indexPath);
        if (action && action.checkable === true)
            action.checked = checked;
    }

    function isSeparator(action) {
        return action
                && (action.separator === true || action.text === "—" || action.text === "-");
    }

    function rebuildMenu() {
        root.clearMenu(root);
        root.populateMenu(root, root.actionsModel || [], root.actionPathPrefix);
    }

    function populateMenu(menu, model, pathPrefix) {
        const objects = [];
        const actions = model || [];

        for (let index = 0; index < actions.length; ++index) {
            const action = actions[index];
            if (!action || action.visible === false)
                continue;

            let object = null;
            if (root.isSeparator(action)) {
                object = separatorComponent.createObject(menu.contentItem);
                if (object)
                    menu.addItem(object);
            } else {
                const children = root.actionChildren(action);
                const path = pathPrefix.concat([index]);
                if (children.length > 0) {
                    object = submenuComponent.createObject(menu, {
                        "title": action.text || "",
                        "enabled": action.enabled !== false,
                        "actionsModel": children,
                        "actionPathPrefix": path
                    });
                    if (object)
                        menu.addMenu(object);
                } else {
                    object = actionComponent.createObject(menu, {
                        "sourceIndex": index,
                        "sourceAction": action,
                        "sourcePath": path,
                        "text": action.text || "",
                        "enabled": action.enabled !== false,
                        "checkable": action.checkable === true,
                        "checked": action.checked === true
                    });
                    if (object)
                        menu.addAction(object);
                }
            }

            if (object)
                objects.push(object);
        }

        menu.generatedObjects = objects;
    }

    function clearMenu(menu) {
        const objects = menu.generatedObjects ? menu.generatedObjects.slice() : [];
        menu.generatedObjects = [];
        for (let index = objects.length - 1; index >= 0; --index) {
            const object = objects[index];
            if (!object)
                continue;

            if (object.generatedKind === root.generatedMenuKind) {
                root.clearMenu(object);
                menu.removeMenu(object);
            } else if (object.generatedKind === root.generatedActionKind) {
                menu.removeAction(object);
            } else {
                menu.removeItem(object);
            }
            object.destroy();
        }
    }

    function triggerAction(index, action, indexPath) {
        const sourceAction = root.actionAtPath(indexPath) || action;
        const text = sourceAction && sourceAction.text ? sourceAction.text : "";
        root.actionTriggered(index, text);
        root.actionSelected(sourceAction, indexPath);
    }
}
