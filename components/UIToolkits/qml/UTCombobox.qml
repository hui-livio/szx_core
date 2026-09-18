/*
 * @file UTCombobox.qml
 * @brief UIToolkits 的 UTCombobox.qml 组件。
 */

import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: root

    // ── Theme ────────────────────────────────────────────────────

    // ── Accessibility ────────────────────────────────────────────
    property string placeholderText: ""
    property string accName: root.displayText || root.placeholderText || ""
    property int accRole: Accessible.ComboBox
    Accessible.name: accName
    Accessible.role: accRole

    // ── Theme-aware colors ───────────────────────────────────────
    property color primaryColor: UTTheme.primaryColor
    property color textColor: UTTheme.primaryTextColor
    property color placeholderColor: UTTheme.secondaryTextColor
    property color fieldBg: UTTheme.surfaceColor
    property color fieldBgDisabled: {
        var c = fieldBg;
        return Qt.rgba(c.r * 0.92, c.g * 0.92, c.b * 0.92, c.a);
    }
    property color borderNormal: UTTheme.borderColor
    property color borderHover: primaryColor
    property color borderFocus: primaryColor
    property color borderDisabled: {
        var c = borderNormal;
        return Qt.rgba(c.r, c.g, c.b, c.a * 0.4);
    }
    property color arrowColor: placeholderColor
    property color arrowHoverColor: primaryColor
    property color popupBg: fieldBg
    property color popupBorder: borderNormal
    property real cornerRadius: UTTheme.radius || 8
    property int popupMinWidth: 180
    property color delegateSelectedBg: {
        var c = primaryColor;
        return Qt.rgba(c.r, c.g, c.b, 0.12);
    }
    property color delegateHoverBg: {
        var c = primaryColor;
        return Qt.rgba(c.r, c.g, c.b, 0.06);
    }
    property string indicatorText: ""
    property color indicatorTextColor: placeholderColor
    property bool filterOnEdit: true
    property bool rememberSearchText: true
    property string searchText: ""
    property bool acceptFreeText: false
    signal editCommitted(string text)
    readonly property int filteredCount: filteredModel.count
    property bool _explicitPopupSelection: false
    property bool _textEdited: false
    property string _pendingEditText: ""

    activeFocusOnTab: true

    ListModel {
        id: filteredModel
    }

    readonly property int filteredCurrentIndex: {
        if (!root.editable || root.currentIndex < 0)
            return -1;
        for (let index = 0; index < filteredModel.count; ++index) {
            if (filteredModel.get(index).sourceIndex === root.currentIndex)
                return index;
        }
        return -1;
    }

    function sourceCount() {
        return root.count;
    }

    function rebuildFilter() {
        if (!root.editable || !root.filterOnEdit) {
            filteredModel.clear();
            return;
        }

        const keyword = String(root.editText || "").trim().toLowerCase();
        filteredModel.clear();
        for (let index = 0; index < root.sourceCount(); ++index) {
            const display = root.textAt(index);
            if (!keyword || display.toLowerCase().indexOf(keyword) >= 0)
                filteredModel.append({ display: display, sourceIndex: index });
        }

        if (root.popup.visible)
            root.resetPopupCurrentIndex();
    }

    function restoreSearchText() {
        if (!root.editable || !root.rememberSearchText || root.searchText === "")
            return;
        if (root.editText === root.searchText)
            return;
        root.editText = root.searchText;
        root.rebuildFilter();
    }

    function resetPopupCurrentIndex() {
        const selectedIndex = root.editable && root.filterOnEdit
                              ? root.filteredCurrentIndex
                              : root.highlightedIndex;
        listView.currentIndex = selectedIndex >= 0
                                ? selectedIndex
                                : (listView.count > 0 ? 0 : -1);
    }

    function movePopupCurrentIndex(step) {
        if (!root.popup.visible)
            root.popup.open();
        if (listView.count <= 0)
            return;

        let nextIndex = listView.currentIndex;
        if (nextIndex < 0)
            nextIndex = step > 0 ? 0 : listView.count - 1;
        else
            nextIndex = Math.max(0, Math.min(listView.count - 1, nextIndex + step));
        listView.currentIndex = nextIndex;
        root._explicitPopupSelection = true;
        listView.positionViewAtIndex(nextIndex, ListView.Contain);
    }

    function activatePopupIndex(index) {
        if (index < 0 || index >= listView.count)
            return;

        const sourceIndex = root.editable && root.filterOnEdit
                            ? filteredModel.get(index).sourceIndex
                            : index;
        root.currentIndex = sourceIndex;
        root._textEdited = false;
        if (root.acceptFreeText)
            root.editCommitted(root.textAt(sourceIndex));
        root.activated(sourceIndex);
        root.popup.close();
    }

    function acceptPopupCurrentIndex() {
        if (root.acceptFreeText && (!root.popup.visible || !root._explicitPopupSelection)) {
            root.commitEdit();
            root.popup.close();
            return;
        }
        if (!root.popup.visible) {
            root.popup.open();
            return;
        }
        if (listView.currentIndex < 0 && listView.count > 0)
            listView.currentIndex = 0;
        root.activatePopupIndex(listView.currentIndex);
    }

    function commitEdit() {
        if (!root.acceptFreeText || !root._textEdited)
            return;
        root._textEdited = false;
        root.editCommitted(root._pendingEditText);
    }

    function resetEditText(text) {
        root._textEdited = false;
        root._pendingEditText = "";
        root._explicitPopupSelection = false;
        root.searchText = "";
        root.editText = text;
    }

    onEditableChanged: root.rebuildFilter()
    onFilterOnEditChanged: root.rebuildFilter()
    onModelChanged: root.rebuildFilter()
    onCountChanged: root.rebuildFilter()
    onEditTextChanged: {
        if (!root.editable)
            return;
        root.rebuildFilter();
    }

    onActiveFocusChanged: {
        if (root.editable && root.activeFocus)
            root.contentItem.forceActiveFocus();
    }

    // ── Sizing ───────────────────────────────────────────────────
    implicitWidth: 200
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 36
    topPadding: 8
    bottomPadding: 8
    font.pixelSize: 14
    flat: false

    // ── contentItem ──────────────────────────────────────────────
    contentItem: TextField {
        topPadding: 0
        bottomPadding: 0
        text: root.editable ? root.editText : root.displayText
        color: {
            if (!root.enabled)
                return root.borderDisabled;
            if (!root.displayText && !root.editText)
                return root.placeholderColor;
            return root.textColor;
        }
        font: root.font
        enabled: root.editable
        focus: root.editable && root.activeFocus
        activeFocusOnTab: root.editable
        autoScroll: root.editable
        selectByMouse: root.selectTextByMouse
        verticalAlignment: Text.AlignVCenter
        clip: true

        background: Item {}

        placeholderText: root.placeholderText || ""
        placeholderTextColor: root.placeholderColor

        onTextEdited: {
            if (!root.editable)
                return;
            if (root.rememberSearchText)
                root.searchText = text;
            root._textEdited = true;
            root._pendingEditText = text;
            root._explicitPopupSelection = false;
            if (!root.popup.visible)
                root.popup.open();
        }

        onEditingFinished: root.commitEdit()

        Keys.onPressed: event => {
            if (!root.editable)
                return;

            switch (event.key) {
            case Qt.Key_Down:
                root.movePopupCurrentIndex(1);
                event.accepted = true;
                break;
            case Qt.Key_Up:
                root.movePopupCurrentIndex(-1);
                event.accepted = true;
                break;
            case Qt.Key_Return:
            case Qt.Key_Enter:
                root.acceptPopupCurrentIndex();
                event.accepted = true;
                break;
            case Qt.Key_Escape:
                if (root.popup.visible) {
                    root.popup.close();
                    event.accepted = true;
                }
                break;
            default:
                break;
            }
        }
    }

    // ── background ───────────────────────────────────────────────
    background: Rectangle {
        id: bgRect
        radius: root.cornerRadius
        color: root.enabled ? root.fieldBg : root.fieldBgDisabled

        HoverHandler {
            id: hoverHandler
        }

        border.width: root.activeFocus ? 2 : 1
        border.color: {
            if (!root.enabled)
                return root.borderDisabled;
            if (root.activeFocus)
                return root.borderFocus;
            if (hoverHandler.hovered)
                return root.borderHover;
            return root.borderNormal;
        }

        Behavior on border.width {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 200
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
    }

    // ── indicator ────────────────────────────────────────────────
    indicator: Text {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 6
        text: root.indicatorText || ""
        font.pixelSize: 14
        color: root.enabled ? (hoverHandler.hovered ? root.arrowHoverColor : root.arrowColor) : root.borderDisabled
        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
    }

    // ── popup ────────────────────────────────────────────────────
    popup: Popup {
        y: root.height - 1
        implicitWidth: Math.max(root.width, root.popupMinWidth)
        implicitHeight: Math.min(listView.contentHeight + padding * 2, 280)
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Math.round(root.cornerRadius * 0.667)
            color: root.popupBg
            border.width: 1
            border.color: root.popupBorder
        }

        contentItem: ListView {
            id: listView
            clip: true
            model: root.editable && root.filterOnEdit ? filteredModel : root.delegateModel
            currentIndex: -1
            implicitHeight: contentHeight

            highlight: Rectangle {
                radius: 4
                color: root.delegateSelectedBg
                Behavior on y {
                    NumberAnimation {
                        duration: 120
                        easing.type: Easing.OutCubic
                    }
                }
            }
            highlightMoveDuration: 120
            highlightResizeDuration: 0

            delegate: ItemDelegate {
                id: delegateRoot
                required property var model
                required property var modelData
                required property int index
                width: ListView.view.width
                implicitHeight: 36
                text: {
                    var d = delegateRoot.modelData;
                    if (d === null || d === undefined)
                        return "";
                    // 字符串/数字数组: modelData 即为文本值本身
                    if (typeof d === "string" || typeof d === "number")
                        return String(d);
                    // QStringListModel: 使用 display 属性
                    if (d.display !== undefined)
                        return String(d.display);
                    // ListModel 或对象数组: 通过 textRole 取值
                    var role = root.textRole || "text";
                    if (d[role] !== undefined)
                        return String(d[role]);
                    return String(d);
                }
                highlighted: ListView.isCurrentItem
                hoverEnabled: true

                contentItem: Text {
                    text: delegateRoot.text
                    color: root.enabled ? root.textColor : root.borderDisabled
                    font: root.font
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                    rightPadding: 12
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: 4
                    color: {
                        if (delegateRoot.highlighted)
                            return root.delegateSelectedBg;
                        if (delegateRoot.hovered)
                            return root.delegateHoverBg;
                        return "transparent";
                    }
                    Behavior on color {
                        ColorAnimation {
                            duration: 100
                        }
                    }
                }

                onClicked: {
                    root.activatePopupIndex(delegateRoot.index);
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }

        onOpened: {
            root._explicitPopupSelection = false;
            root.restoreSearchText();
            root.rebuildFilter();
            root.resetPopupCurrentIndex();
            if (root.editable)
                root.contentItem.forceActiveFocus();
        }
        onClosed: {
            if (root.editable && root.activeFocus)
                root.contentItem.forceActiveFocus();
        }
    }

    // ── Hover cursor ────────────────────────────────────────────
    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
