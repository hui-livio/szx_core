pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits
import SzxLicenseIssuerBackend

UTAppWindow {
    id: window

    visible: true
    width: 1100
    height: 720
    minimumWidth: 860
    minimumHeight: 560
    title: qsTr("SZX 许可证签发工具")

    property int currentPage: 0
    readonly property var projectCandidates: {
        var values = [];
        for (var i = 0; i < IssuerController.keyRecords.length; ++i) {
            var projectId = String(IssuerController.keyRecords[i].projectId);
            if (projectId !== "" && values.indexOf(projectId) < 0) {
                values.push(projectId);
            }
        }
        return values;
    }
    readonly property var keyCandidates: {
        var values = [];
        for (var i = 0; i < IssuerController.keyRecords.length; ++i) {
            var record = IssuerController.keyRecords[i];
            values.push({
                label: qsTr("%1 / %2").arg(String(record.projectId)).arg(String(record.id)),
                privatePath: String(record.privatePath),
                publicPath: String(record.publicPath)
            });
        }
        return values;
    }
    readonly property var pages: [
        {
            title: qsTr("主页"),
            description: qsTr("软授权工具框架")
        },
        {
            title: qsTr("密钥管理"),
            description: qsTr("生成并检查 RSA-3072 签发密钥")
        },
        {
            title: qsTr("签发许可证"),
            description: qsTr("根据请求文件签发许可证")
        },
        {
            title: qsTr("验证许可证"),
            description: qsTr("许可证验证功能将在后续版本实现")
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: window.contentMargin
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                color: window.theme.surfaceColor

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    UTLabel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("许可证工具")
                        textRole: UTLabel.TextRole.H4
                        color: window.theme.primaryColor
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: window.theme.borderColor
                    }

                    ListView {
                        id: navigation
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: window.pages

                        delegate: ItemDelegate {
                            id: navigationItem
                            required property int index
                            required property var modelData

                            width: navigation.width
                            highlighted: index === window.currentPage
                            implicitHeight: 48
                            padding: 16
                            background: Rectangle {
                                color: navigationItem.highlighted ? window.theme.primaryColor : "transparent"
                                opacity: navigationItem.highlighted ? 0.18 : 1
                            }
                            contentItem: UTLabel {
                                text: navigationItem.modelData.title
                                color: navigationItem.highlighted ? window.theme.primaryColor : window.theme.primaryTextColor
                                textRole: UTLabel.TextRole.Body
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: window.currentPage = index
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: window.theme.backgroundColor

                StackLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    currentIndex: window.currentPage

                    PlaceholderPage {
                        pageTitle: qsTr("主页")
                        description: qsTr("选择左侧功能开始操作")
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 32
                        spacing: 16
                        UTLabel {
                            text: qsTr("密钥管理")
                            textRole: UTLabel.TextRole.H2
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            UTButton {
                                Layout.preferredWidth: 104
                                text: qsTr("导入密钥对")
                                textElide: Text.ElideNone
                                enabled: !IssuerController.busy
                                onClicked: keyDialog.open()
                            }
                            UTButton {
                                Layout.preferredWidth: 104
                                text: qsTr("生成密钥对")
                                textElide: Text.ElideNone
                                enabled: !IssuerController.busy
                                onClicked: generateDialog.open()
                            }
                            UTLineEdit {
                                id: searchField
                                Layout.fillWidth: true
                                placeholderText: qsTr("查询：密钥对 ID、项目 ID 或路径")
                            }
                        }
                        UTLabel {
                            text: IssuerController.statusMessage
                            color: IssuerController.status === "error" ? "#d9534f" : UTTheme.secondaryTextColor
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            UTLabel {
                                Layout.preferredWidth: 150
                                text: qsTr("密钥对 ID")
                                textRole: UTLabel.TextRole.Caption
                            }
                            UTLabel {
                                Layout.preferredWidth: 150
                                text: qsTr("项目 ID")
                                textRole: UTLabel.TextRole.Caption
                            }
                            UTLabel {
                                Layout.fillWidth: true
                                text: qsTr("密钥信息")
                                textRole: UTLabel.TextRole.Caption
                            }
                            UTLabel {
                                Layout.preferredWidth: 180
                                text: qsTr("操作")
                                textRole: UTLabel.TextRole.Caption
                            }
                        }
                        ListView {
                            id: keyList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: IssuerController.keyRecords.filter(function (record) {
                                var query = searchField.text.trim().toLowerCase();
                                if (query === "")
                                    return true;
                                return String(record.id).toLowerCase().indexOf(query) >= 0 || String(record.projectId).toLowerCase().indexOf(query) >= 0 || String(record.privatePath).toLowerCase().indexOf(query) >= 0 || String(record.publicPath).toLowerCase().indexOf(query) >= 0;
                            })
                            delegate: Rectangle {
                                required property var modelData
                                width: keyList.width
                                height: 44
                                color: "transparent"
                                border.width: 0
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: window.theme.borderColor
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 8
                                    UTLabel {
                                        Layout.preferredWidth: 150
                                        text: modelData.id
                                        color: UTTheme.primaryTextColor
                                    }
                                    UTLabel {
                                        Layout.preferredWidth: 150
                                        text: modelData.projectId
                                        color: UTTheme.primaryTextColor
                                    }
                                    UTLabel {
                                        Layout.fillWidth: true
                                        text: qsTr("已归档")
                                        color: UTTheme.secondaryTextColor
                                    }
                                    RowLayout {
                                        Layout.preferredWidth: 180
                                        spacing: 8
                                        UTButton {
                                            Layout.preferredWidth: 78
                                            text: qsTr("编辑")
                                            textElide: Text.ElideNone
                                            enabled: !IssuerController.busy
                                            onClicked: keyDialog.editRecord(modelData)
                                        }
                                        UTButton {
                                            Layout.preferredWidth: 78
                                            text: qsTr("删除")
                                            textElide: Text.ElideNone
                                            enabled: !IssuerController.busy
                                            onClicked: IssuerController.removeKeyRecord(modelData.projectId, modelData.id)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        readonly property var selectedKey: {
                            const index = issueKeyField.currentIndex;
                            return index >= 0 && index < window.keyCandidates.length ? window.keyCandidates[index] : null;
                        }
                        function issueDeadlineEpoch() {
                            const value = issueDeadline.dateTimeValue.trim();
                            if (value === "" || !issueDeadline.valueValid)
                                return NaN;
                            const date = new Date(value.replace(" ", "T"));
                            return isNaN(date.getTime()) ? NaN : Math.floor(date.getTime() / 1000);
                        }
                        readonly property string issueValidationMessage: {
                            if (requestPath.text.trim() === "")
                                return qsTr("请输入请求文件路径。");
                            if (!selectedKey)
                                return qsTr("请选择签发密钥对。");
                            if (licenseId.text.trim() === "")
                                return qsTr("请输入许可证 ID。");
                            if (issuerName.text.trim() === "")
                                return qsTr("请输入签发方。");
                            if (issueOutput.text.trim() === "")
                                return qsTr("请输入输出许可证路径。");
                            if (!issuePermanent.checked) {
                                const expiresAt = issueDeadlineEpoch();
                                if (!isFinite(expiresAt))
                                    return qsTr("请选择有效的截止时间。");
                                if (expiresAt <= Math.floor(Date.now() / 1000))
                                    return qsTr("截止时间必须晚于当前时间。");
                            }
                            return "";
                        }
                        readonly property bool canIssue: !IssuerController.busy && issueValidationMessage === ""
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 32
                        spacing: 14
                        UTLabel {
                            text: qsTr("签发许可证")
                            textRole: UTLabel.TextRole.H2
                        }
                        UTLabel {
                            Layout.fillWidth: true
                            text: qsTr("选择请求文件和签发密钥，填写许可证元数据后生成许可证文件。")
                            color: UTTheme.secondaryTextColor
                            wrapMode: Text.WordWrap
                        }
                        UTLineEdit {
                            id: requestPath
                            Layout.fillWidth: true
                            placeholderText: qsTr("请求文件路径（必填）")
                        }
                        UTCombobox {
                            id: issueKeyField
                            Layout.fillWidth: true
                            model: window.keyCandidates
                            textRole: "label"
                            currentIndex: -1
                            placeholderText: qsTr("签发密钥对（必选）")
                        }
                        UTLineEdit {
                            id: licenseId
                            Layout.fillWidth: true
                            placeholderText: qsTr("许可证 ID")
                        }
                        UTLineEdit {
                            id: issuerName
                            Layout.fillWidth: true
                            placeholderText: qsTr("签发方")
                        }
                        UTLineEdit {
                            id: issueOutput
                            Layout.fillWidth: true
                            placeholderText: qsTr("输出许可证路径")
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            UTLabel {
                                Layout.preferredWidth: 72
                                text: qsTr("有效期")
                                color: UTTheme.secondaryTextColor
                            }
                            UTCheckBox {
                                id: issuePermanent
                                text: qsTr("永久")
                                checked: true
                            }
                            UTDateTimePicker {
                                id: issueDeadline
                                Layout.fillWidth: true
                                enabled: !issuePermanent.checked
                                showSeconds: false
                                placeholderText: qsTr("选择截止日期和时间")
                                invalidDateTimeText: qsTr("截止时间格式无效")
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }
                        UTButton {
                            Layout.preferredWidth: 110
                            text: qsTr("签发")
                            textElide: Text.ElideNone
                            enabled: canIssue
                            onClicked: {
                                if (!canIssue)
                                    return;
                                var options = {
                                    licenseId: licenseId.text.trim(),
                                    issuer: issuerName.text.trim()
                                };
                                if (!issuePermanent.checked)
                                    options.expiresAtUtc = issueDeadlineEpoch();
                                IssuerController.issueLicense(requestPath.text.trim(), selectedKey.privatePath, selectedKey.publicPath, options, issueOutput.text, false);
                            }
                        }
                        UTLabel {
                            Layout.fillWidth: true
                            text: IssuerController.statusMessage
                            color: IssuerController.status === "error" ? "#d9534f" : UTTheme.secondaryTextColor
                            wrapMode: Text.WordWrap
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 32
                        spacing: 14
                        UTLabel {
                            text: qsTr("验证许可证")
                            textRole: UTLabel.TextRole.H2
                        }
                        UTLineEdit {
                            id: verifyPath
                            Layout.fillWidth: true
                            placeholderText: qsTr("许可证路径")
                        }
                        UTLineEdit {
                            id: verifyPublic
                            Layout.fillWidth: true
                            placeholderText: qsTr("公钥路径")
                        }
                        UTLineEdit {
                            id: expectedProduct
                            Layout.fillWidth: true
                            placeholderText: qsTr("预期产品（可选）")
                        }
                        UTButton {
                            text: qsTr("验证")
                            onClicked: IssuerController.verifyLicense(verifyPath.text, verifyPublic.text, expectedProduct.text)
                        }
                        UTLabel {
                            text: IssuerController.statusMessage
                            color: IssuerController.status === "error" ? "#d9534f" : UTTheme.secondaryTextColor
                        }
                    }
                }
            }
        }
    }

    UTDialog {
        id: keyDialog
        property var editingRecord: null
        titleText: editingRecord ? qsTr("编辑密钥对") : qsTr("导入密钥对")
        modal: true
        initialDialogWidth: 520
        initialDialogHeight: 500
        width: 520
        height: 500
        standardButtons: Dialog.NoButton
        function editRecord(record) {
            editingRecord = record;
            projectField.currentIndex = projectCandidates.indexOf(record.projectId);
            projectField.editText = record.projectId;
            pairIdField.text = record.id;
            privateField.text = record.privatePath;
            publicField.text = record.publicPath;
            open();
        }
        onAccepted: {
            var projectId = projectField.editText.trim();
            if (editingRecord)
                IssuerController.updateKeyRecord(editingRecord.projectId, editingRecord.id, projectId, pairIdField.text, privateField.text, publicField.text);
            else
                IssuerController.importKeyPair(privateField.text, publicField.text, projectId, pairIdField.text, false);
            editingRecord = null;
        }
        onRejected: editingRecord = null
        dialogContent: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10
            UTCombobox {
                id: projectField
                Layout.fillWidth: true
                editable: true
                model: window.projectCandidates
                currentIndex: -1
                placeholderText: qsTr("所属项目 ID")
            }
            UTLineEdit {
                id: pairIdField
                Layout.fillWidth: true
                placeholderText: qsTr("密钥对 ID")
            }
            UTLineEdit {
                id: privateField
                Layout.fillWidth: true
                placeholderText: qsTr("私钥文件路径")
            }
            UTLineEdit {
                id: publicField
                Layout.fillWidth: true
                placeholderText: qsTr("公钥文件路径")
            }
            RowLayout {
                Layout.fillWidth: true
                Item {
                    Layout.fillWidth: true
                }
                UTButton {
                    text: qsTr("取消")
                    buttonType: UTButton.ButtonType.Outlined
                    enabled: !IssuerController.busy
                    onClicked: keyDialog.reject()
                }
                UTButton {
                    text: qsTr("确定")
                    buttonType: UTButton.ButtonType.Filled
                    enabled: !IssuerController.busy
                    onClicked: keyDialog.accept()
                }
            }
        }
    }

    UTDialog {
        id: generateDialog
        titleText: qsTr("生成 RSA-3072 密钥对")
        modal: true
        initialDialogWidth: 520
        initialDialogHeight: 380
        width: 520
        height: 380
        standardButtons: Dialog.NoButton
        onAccepted: {
            IssuerController.generateProjectKeyPair(generateProjectField.editText.trim(), generatePairIdField.text, generateOverwrite.checked);
        }
        dialogContent: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10
            UTCombobox {
                id: generateProjectField
                Layout.fillWidth: true
                editable: true
                model: window.projectCandidates
                currentIndex: -1
                placeholderText: qsTr("所属项目 ID")
            }
            UTLineEdit {
                id: generatePairIdField
                Layout.fillWidth: true
                placeholderText: qsTr("密钥对 ID")
            }
            UTCheckBox {
                id: generateOverwrite
                text: qsTr("覆盖已存在的密钥文件")
            }
            RowLayout {
                Layout.fillWidth: true
                Item {
                    Layout.fillWidth: true
                }
                UTButton {
                    text: qsTr("取消")
                    buttonType: UTButton.ButtonType.Outlined
                    enabled: !IssuerController.busy
                    onClicked: generateDialog.reject()
                }
                UTButton {
                    text: qsTr("确定")
                    buttonType: UTButton.ButtonType.Filled
                    enabled: !IssuerController.busy
                    onClicked: generateDialog.accept()
                }
            }
        }
    }
}
