import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import UIToolkits

UTAppWindow {
    id: window

    visible: true
    width: 1100
    height: 720
    minimumWidth: 860
    minimumHeight: 560
    title: qsTr("SZX 许可证签发工具")

    property int currentPage: 0
    readonly property var pages: [
        { title: qsTr("主页"), description: qsTr("软授权工具框架") },
        { title: qsTr("密钥管理"), description: qsTr("密钥管理功能将在后续版本实现") },
        { title: qsTr("签发许可证"), description: qsTr("许可证签发功能将在后续版本实现") },
        { title: qsTr("验证许可证"), description: qsTr("许可证验证功能将在后续版本实现") }
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
                    currentIndex: window.currentPage

                    PlaceholderPage { pageTitle: qsTr("主页"); description: qsTr("选择左侧功能开始操作") }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 32
                        spacing: 16
                        UTLabel { text: qsTr("密钥管理"); textRole: UTLabel.TextRole.H2 }
                        UTLineEdit { id: privatePath; Layout.fillWidth: true; placeholderText: qsTr("私钥路径") }
                        UTLineEdit { id: publicPath; Layout.fillWidth: true; placeholderText: qsTr("公钥路径") }
                        RowLayout {
                            spacing: 12
                            UTButton { text: qsTr("生成 RSA-3072 密钥"); onClicked: issuerController.generateKeyPair(privatePath.text, publicPath.text, false) }
                            UTButton { text: qsTr("检查密钥"); onClicked: issuerController.inspectKeyPair(privatePath.text, publicPath.text) }
                        }
                        UTLabel { text: issuerController.statusMessage; color: issuerController.status === "error" ? "#d9534f" : UTTheme.secondaryTextColor }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 32
                        spacing: 14
                        UTLabel { text: qsTr("签发许可证"); textRole: UTLabel.TextRole.H2 }
                        UTLineEdit { id: requestPath; Layout.fillWidth: true; placeholderText: qsTr("请求文件路径") }
                        UTLineEdit { id: issuePrivate; Layout.fillWidth: true; placeholderText: qsTr("私钥路径") }
                        UTLineEdit { id: issuePublic; Layout.fillWidth: true; placeholderText: qsTr("公钥路径") }
                        UTLineEdit { id: licenseId; Layout.fillWidth: true; placeholderText: qsTr("许可证 ID") }
                        UTLineEdit { id: issuerName; Layout.fillWidth: true; placeholderText: qsTr("签发方") }
                        UTLineEdit { id: issueOutput; Layout.fillWidth: true; placeholderText: qsTr("输出许可证路径") }
                        UTButton {
                            text: qsTr("签发")
                            onClicked: issuerController.issueLicense(requestPath.text, issuePrivate.text, issuePublic.text,
                                                                      {licenseId: licenseId.text, issuer: issuerName.text}, issueOutput.text, false)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 32
                        spacing: 14
                        UTLabel { text: qsTr("验证许可证"); textRole: UTLabel.TextRole.H2 }
                        UTLineEdit { id: verifyPath; Layout.fillWidth: true; placeholderText: qsTr("许可证路径") }
                        UTLineEdit { id: verifyPublic; Layout.fillWidth: true; placeholderText: qsTr("公钥路径") }
                        UTLineEdit { id: expectedProduct; Layout.fillWidth: true; placeholderText: qsTr("预期产品（可选）") }
                        UTButton { text: qsTr("验证"); onClicked: issuerController.verifyLicense(verifyPath.text, verifyPublic.text, expectedProduct.text) }
                        UTLabel { text: issuerController.statusMessage; color: issuerController.status === "error" ? "#d9534f" : UTTheme.secondaryTextColor }
                    }
                }
            }
        }
    }
}
