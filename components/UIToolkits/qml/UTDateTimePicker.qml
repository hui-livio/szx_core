/*
 * @file UTDateTimePicker.qml
 * @brief 桌面端日期时间选择器，支持日历、键盘导航与显式提交。
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Control {
    id: root

    property string dateTimeValue: ""
    property bool showSeconds: false
    property int minYear: 1970
    property int maxYear: 2100
    property int minuteStep: 1
    property bool showNowButton: true
    property string placeholderText: "Select date and time"
    property string openPickerAccessibleName: "Open date and time picker"
    property string previousMonthAccessibleName: "Previous month"
    property string nextMonthAccessibleName: "Next month"
    property string timeLabel: "Time"
    property string hourLabel: "Hour"
    property string minuteLabel: "Minute"
    property string secondLabel: "Second"
    property string nowText: "Now"
    property string cancelText: "Cancel"
    property string applyText: "Apply"
    property string invalidDateTimeText: "Invalid date and time"

    signal valueChanged(string value)

    readonly property var _appWindow: Window.window
    readonly property bool popupVisible: pickerPopup.visible
    readonly property bool valueValid: _valueValid
    readonly property string validationMessage: valueValid ? "" : invalidDateTimeText

    property date _selectedDate: new Date()
    property int _selectedHour: 0
    property int _selectedMinute: 0
    property int _selectedSecond: 0
    property int _viewYear: _selectedDate.getFullYear()
    property int _viewMonth: _selectedDate.getMonth()
    property bool _syncing: false
    property bool _valueValid: true
    property string _displayValue: ""

    implicitWidth: 360
    implicitHeight: 44
    leftPadding: 12
    rightPadding: 10
    topPadding: 0
    bottomPadding: 0
    activeFocusOnTab: true

    Accessible.role: Accessible.ComboBox
    Accessible.name: _displayValue === "" ? placeholderText : _displayValue
    Accessible.description: validationMessage !== "" ? validationMessage : openPickerAccessibleName

    function pad(number) {
        return (number < 10 ? "0" : "") + number;
    }

    function daysInMonth(year, month) {
        return new Date(year, month + 1, 0).getDate();
    }

    function createValidDate(year, month, day) {
        if (!Number.isInteger(year) || !Number.isInteger(month) || !Number.isInteger(day) || year < minYear || year > maxYear || month < 1 || month > 12 || day < 1)
            return null;
        const candidate = new Date(year, month - 1, day);
        if (candidate.getFullYear() !== year || candidate.getMonth() !== month - 1 || candidate.getDate() !== day)
            return null;
        return candidate;
    }

    function clampDate(date) {
        const minDate = new Date(minYear, 0, 1);
        const maxDate = new Date(maxYear, 11, 31);
        if (date < minDate)
            return minDate;
        if (date > maxDate)
            return maxDate;
        return date;
    }

    function dateToString(date) {
        return date.getFullYear() + "-" + pad(date.getMonth() + 1) + "-" + pad(date.getDate());
    }

    function timeToString(hour, minute, second) {
        const base = pad(hour) + ":" + pad(minute);
        return showSeconds ? base + ":" + pad(second) : base;
    }

    function dateTimeToString() {
        return dateToString(_selectedDate) + " " + timeToString(_selectedHour, _selectedMinute, _selectedSecond);
    }

    function localizedDateTimeString() {
        return _selectedDate.toLocaleDateString(Qt.locale(), Locale.ShortFormat) + "  " + timeToString(_selectedHour, _selectedMinute, _selectedSecond);
    }

    function localizedSelectedDate() {
        return _selectedDate.toLocaleDateString(Qt.locale(), Locale.LongFormat);
    }

    function parseDateTime(text) {
        const now = new Date();
        const fallback = {
            date: new Date(now.getFullYear(), now.getMonth(), now.getDate()),
            hour: now.getHours(),
            minute: now.getMinutes(),
            second: now.getSeconds(),
            valid: String(text || "").trim() === ""
        };
        const raw = String(text || "").trim();
        if (raw === "")
            return fallback;

        const parts = raw.indexOf("T") >= 0 ? raw.split("T") : raw.split(/\s+/);
        if (parts.length !== 2)
            return fallback;
        const dateParts = parts[0].split("-");
        const timeParts = parts[1].split(":");
        if (dateParts.length !== 3 || timeParts.length < 2 || timeParts.length > 3)
            return fallback;

        const date = createValidDate(Number(dateParts[0]), Number(dateParts[1]), Number(dateParts[2]));
        const hour = Number(timeParts[0]);
        const minute = Number(timeParts[1]);
        const second = timeParts.length === 3 ? Number(timeParts[2]) : 0;
        if (!date || !Number.isInteger(hour) || !Number.isInteger(minute) || !Number.isInteger(second) || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
            return fallback;

        return {
            date: date,
            hour: hour,
            minute: minute,
            second: second,
            valid: true
        };
    }

    function syncFromValue() {
        if (_syncing)
            return;
        _syncing = true;
        const parsed = parseDateTime(dateTimeValue);
        _selectedDate = parsed.date;
        _selectedHour = parsed.hour;
        _selectedMinute = parsed.minute;
        _selectedSecond = parsed.second;
        _viewYear = _selectedDate.getFullYear();
        _viewMonth = _selectedDate.getMonth();
        _valueValid = parsed.valid;
        _displayValue = dateTimeValue === "" ? "" : (parsed.valid ? localizedDateTimeString() : dateTimeValue);
        hourInput.value = _selectedHour;
        minuteInput.value = _selectedMinute;
        secondInput.value = _selectedSecond;
        _syncing = false;
    }

    function setSelectedDate(date) {
        _selectedDate = clampDate(date);
        _viewYear = _selectedDate.getFullYear();
        _viewMonth = _selectedDate.getMonth();
    }

    function moveSelectedDate(days) {
        setSelectedDate(new Date(_selectedDate.getFullYear(), _selectedDate.getMonth(), _selectedDate.getDate() + days));
    }

    function shiftSelectedMonth(delta) {
        const first = new Date(_selectedDate.getFullYear(), _selectedDate.getMonth() + delta, 1);
        const day = Math.min(_selectedDate.getDate(), daysInMonth(first.getFullYear(), first.getMonth()));
        setSelectedDate(new Date(first.getFullYear(), first.getMonth(), day));
    }

    function shiftViewMonth(delta) {
        const target = new Date(_viewYear, _viewMonth + delta, 1);
        if (target.getFullYear() < minYear || target.getFullYear() > maxYear)
            return;
        _viewYear = target.getFullYear();
        _viewMonth = target.getMonth();
    }

    function selectNow() {
        const now = new Date();
        setSelectedDate(new Date(now.getFullYear(), now.getMonth(), now.getDate()));
        hourInput.value = now.getHours();
        minuteInput.value = now.getMinutes();
        secondInput.value = now.getSeconds();
    }

    function applySelectionAndClose() {
        _selectedHour = hourInput.value;
        _selectedMinute = minuteInput.value;
        _selectedSecond = secondInput.value;
        const next = dateTimeToString();
        _valueValid = true;
        _displayValue = localizedDateTimeString();
        if (dateTimeValue !== next) {
            dateTimeValue = next;
            valueChanged(next);
        }
        pickerPopup.close();
    }

    function openPicker() {
        syncFromValue();
        pickerPopup.open();
    }

    onDateTimeValueChanged: syncFromValue()
    onShowSecondsChanged: syncFromValue()

    Keys.onReturnPressed: openPicker()
    Keys.onEnterPressed: openPicker()
    Keys.onSpacePressed: openPicker()
    Keys.onDownPressed: openPicker()

    contentItem: RowLayout {
        spacing: 8

        UTLabel {
            Layout.fillWidth: true
            text: root._displayValue === "" ? root.placeholderText : root._displayValue
            color: root._displayValue === "" || !root.valueValid ? UTTheme.secondaryTextColor : UTTheme.primaryTextColor
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Item {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Accessible.ignored: true

            Rectangle {
                anchors.fill: parent
                anchors.margins: 3
                radius: 3
                color: "transparent"
                border.width: 1.5
                border.color: root.activeFocus || fieldHover.hovered ? UTTheme.primaryColor : UTTheme.secondaryTextColor

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 3
                    height: 2
                    radius: 1
                    color: parent.border.color
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 2
                    Repeater {
                        model: 2
                        Rectangle {
                            required property int index
                            width: 3
                            height: 3
                            radius: 1
                            color: UTTheme.secondaryTextColor
                        }
                    }
                }
            }
        }
    }

    background: Rectangle {
        radius: UTTheme.radius || 8
        color: UTTheme.surfaceColor
        border.width: root.activeFocus ? 2 : 1
        border.color: {
            if (!root.valueValid)
                return UTTheme.errorColor;
            if (root.activeFocus || fieldHover.hovered || root.popupVisible)
                return UTTheme.primaryColor;
            return UTTheme.borderColor;
        }
    }

    HoverHandler {
        id: fieldHover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: root.openPicker()
    }

    Popup {
        id: pickerPopup
        y: root.height + 6
        width: Math.min(420, root._appWindow ? root._appWindow.width - 24 : 420)
        padding: 16
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAboutToShow: {
            const globalPosition = root.mapToItem(null, 0, 0);
            const windowWidth = root._appWindow ? root._appWindow.width : 9999;
            const windowHeight = root._appWindow ? root._appWindow.height : 9999;
            const globalBottom = root.mapToItem(null, 0, root.height).y;
            const popupHeight = pickerPopup.implicitHeight > 0 ? pickerPopup.implicitHeight : 560;
            const targetY = globalBottom + popupHeight + 10 > windowHeight ? globalPosition.y - popupHeight - 6 : globalBottom + 6;
            pickerPopup.y = Math.max(-globalPosition.y, Math.min(targetY - globalPosition.y, windowHeight - popupHeight - globalPosition.y));
            pickerPopup.x = Math.max(-globalPosition.x, Math.min(0, windowWidth - globalPosition.x - pickerPopup.width));
        }
        onOpened: calendarGrid.forceActiveFocus()
        onClosed: root.syncFromValue()

        background: Rectangle {
            radius: Math.round((UTTheme.radius || 8) * 1.25)
            color: UTTheme.surfaceColor
            border.width: 1
            border.color: UTTheme.borderColor
        }

        contentItem: ColumnLayout {
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                UTRoundButton {
                    text: "‹"
                    buttonSize: 36
                    variant: UTRoundButton.ButtonVariant.Text
                    accName: root.previousMonthAccessibleName
                    onClicked: root.shiftViewMonth(-1)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    UTLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: Qt.locale().monthName(root._viewMonth) + " " + root._viewYear
                        font.weight: Font.DemiBold
                        font.pixelSize: 16
                    }
                    UTLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: root.localizedSelectedDate()
                        color: UTTheme.secondaryTextColor
                        font.pixelSize: 12
                    }
                }

                UTRoundButton {
                    text: "›"
                    buttonSize: 36
                    variant: UTRoundButton.ButtonVariant.Text
                    accName: root.nextMonthAccessibleName
                    onClicked: root.shiftViewMonth(1)
                }
            }

            DayOfWeekRow {
                locale: Qt.locale()
                Layout.fillWidth: true
                delegate: UTLabel {
                    required property var model
                    text: model.shortName
                    horizontalAlignment: Text.AlignHCenter
                    color: UTTheme.secondaryTextColor
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 228
                radius: UTTheme.radius || 8
                color: "transparent"
                border.width: calendarGrid.activeFocus ? 1 : 0
                border.color: UTTheme.primaryColor

                MonthGrid {
                    id: calendarGrid
                    anchors.fill: parent
                    anchors.margins: 3
                    month: root._viewMonth
                    year: root._viewYear
                    locale: Qt.locale()
                    activeFocusOnTab: true

                    Keys.onLeftPressed: function (event) {
                        root.moveSelectedDate(-1);
                        event.accepted = true;
                    }
                    Keys.onRightPressed: function (event) {
                        root.moveSelectedDate(1);
                        event.accepted = true;
                    }
                    Keys.onUpPressed: function (event) {
                        root.moveSelectedDate(-7);
                        event.accepted = true;
                    }
                    Keys.onDownPressed: function (event) {
                        root.moveSelectedDate(7);
                        event.accepted = true;
                    }
                    Keys.onPressed: function (event) {
                        if (event.key === Qt.Key_PageUp) {
                            root.shiftSelectedMonth(-1);
                            event.accepted = true;
                        } else if (event.key === Qt.Key_PageDown) {
                            root.shiftSelectedMonth(1);
                            event.accepted = true;
                        }
                    }

                    delegate: Button {
                        id: dayCell
                        required property var model
                        readonly property bool sameMonth: model.month === calendarGrid.month
                        readonly property bool isSelected: model.year === root._selectedDate.getFullYear() && model.month === root._selectedDate.getMonth() && model.day === root._selectedDate.getDate()
                        readonly property date cellDate: new Date(model.year, model.month, model.day)
                        readonly property date today: new Date()
                        readonly property bool isToday: model.year === today.getFullYear() && model.month === today.getMonth() && model.day === today.getDate()

                        enabled: sameMonth
                        activeFocusOnTab: false
                        Accessible.name: cellDate.toLocaleDateString(Qt.locale(), Locale.LongFormat)
                        onClicked: root.setSelectedDate(cellDate)

                        contentItem: UTLabel {
                            text: String(dayCell.model.day)
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: dayCell.isSelected ? "white" : (dayCell.sameMonth ? UTTheme.primaryTextColor : UTTheme.secondaryTextColor)
                            font.pixelSize: 13
                            font.weight: dayCell.isSelected || dayCell.isToday ? Font.DemiBold : Font.Normal
                        }
                        background: Rectangle {
                            radius: Math.round((UTTheme.radius || 8) * 0.75)
                            color: dayCell.isSelected ? UTTheme.primaryColor : (dayCell.hovered && dayCell.sameMonth ? Qt.rgba(UTTheme.primaryColor.r, UTTheme.primaryColor.g, UTTheme.primaryColor.b, 0.10) : "transparent")
                            border.width: dayCell.isToday && !dayCell.isSelected ? 1 : 0
                            border.color: UTTheme.primaryColor
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: UTTheme.borderColor
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                UTLabel {
                    text: root.timeLabel
                    font.weight: Font.DemiBold
                }

                Item {
                    Layout.fillWidth: true
                }

                ColumnLayout {
                    spacing: 4
                    UTLabel {
                        text: root.hourLabel
                        color: UTTheme.secondaryTextColor
                        font.pixelSize: 12
                    }
                    UTSpinBox {
                        id: hourInput
                        Layout.preferredWidth: 76
                        from: 0
                        to: 23
                        editable: true
                        accName: root.hourLabel
                        textFromValue: function (value, locale) {
                            return root.pad(value);
                        }
                        valueFromText: function (text, locale) {
                            return Number(text);
                        }
                        validator: IntValidator {
                            bottom: 0
                            top: 23
                        }
                        onValueChanged: if (!root._syncing)
                            root._selectedHour = value
                    }
                }

                UTLabel {
                    text: ":"
                    font.pixelSize: 18
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: 8
                }

                ColumnLayout {
                    spacing: 4
                    UTLabel {
                        text: root.minuteLabel
                        color: UTTheme.secondaryTextColor
                        font.pixelSize: 12
                    }
                    UTSpinBox {
                        id: minuteInput
                        Layout.preferredWidth: 76
                        from: 0
                        to: 59
                        stepSize: Math.max(1, root.minuteStep)
                        editable: true
                        accName: root.minuteLabel
                        textFromValue: function (value, locale) {
                            return root.pad(value);
                        }
                        valueFromText: function (text, locale) {
                            return Number(text);
                        }
                        validator: IntValidator {
                            bottom: 0
                            top: 59
                        }
                        onValueChanged: if (!root._syncing)
                            root._selectedMinute = value
                    }
                }

                UTLabel {
                    visible: root.showSeconds
                    text: ":"
                    font.pixelSize: 18
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: 8
                }

                ColumnLayout {
                    visible: root.showSeconds
                    spacing: 4
                    UTLabel {
                        text: root.secondLabel
                        color: UTTheme.secondaryTextColor
                        font.pixelSize: 12
                    }
                    UTSpinBox {
                        id: secondInput
                        Layout.preferredWidth: 76
                        from: 0
                        to: 59
                        editable: true
                        accName: root.secondLabel
                        textFromValue: function (value, locale) {
                            return root.pad(value);
                        }
                        valueFromText: function (text, locale) {
                            return Number(text);
                        }
                        validator: IntValidator {
                            bottom: 0
                            top: 59
                        }
                        onValueChanged: if (!root._syncing)
                            root._selectedSecond = value
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                UTButton {
                    visible: root.showNowButton
                    text: root.nowText
                    buttonType: UTButton.ButtonType.Text
                    onClicked: root.selectNow()
                }

                Item {
                    Layout.fillWidth: true
                }

                UTButton {
                    text: root.cancelText
                    buttonType: UTButton.ButtonType.Surface
                    onClicked: pickerPopup.close()
                }

                UTButton {
                    text: root.applyText
                    buttonType: UTButton.ButtonType.Filled
                    onClicked: root.applySelectionAndClose()
                }
            }
        }
    }

    Component.onCompleted: syncFromValue()
}
