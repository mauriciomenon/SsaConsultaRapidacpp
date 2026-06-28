pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SsaConsultaRapida

Window {
    id: root
    objectName: DesktopSmokeObjectNames.themeDialog

    required property var viewModel
    property string originalTheme: ""
    property string pendingTheme: viewModel ? viewModel.ui.theme : ""

    title: "Personalizar tema"
    modality: Qt.ApplicationModal
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
    color: Theme.window
    minimumWidth: 520
    minimumHeight: 420
    width: 560
    height: 460

    function open() {
        root.originalTheme = root.viewModel.ui.theme;
        root.pendingTheme = root.originalTheme;
        root.show();
        root.raise();
        root.requestActivate();
    }

    function accept() {
        root.viewModel.ui.theme = root.pendingTheme;
        root.originalTheme = root.pendingTheme;
        root.close();
    }

    function reject() {
        root.viewModel.ui.theme = root.originalTheme;
        root.close();
    }

    onClosing: function (close) {
        if (root.viewModel.ui.theme !== root.originalTheme) {
            root.viewModel.ui.theme = root.originalTheme;
        }
        close.accepted = true;
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: Theme.cardGap

            Label {
                text: "Selecione um tema"
                color: Theme.text
                font.bold: true
                font.pixelSize: 14
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.cardGap

                Rectangle {
                    Layout.preferredWidth: 180
                    Layout.fillHeight: true
                    color: Theme.surface
                    border.color: Theme.border
                    radius: Theme.radius

                    ListView {
                        id: themeList
                        anchors.fill: parent
                        anchors.margins: 6
                        clip: true
                        model: Theme.themeOptions
                        currentIndex: Theme.themeOptions.indexOf(root.pendingTheme)
                        delegate: ItemDelegate {
                            id: themeDelegate
                            required property string modelData
                            required property int index
                            width: themeList.width
                            text: themeDelegate.modelData
                            highlighted: ListView.isCurrentItem
                            onClicked: {
                                root.pendingTheme = themeDelegate.modelData;
                                root.viewModel.ui.theme = themeDelegate.modelData;
                            }
                            contentItem: Label {
                                text: themeDelegate.text
                                color: themeDelegate.highlighted ? Theme.accentText : Theme.text
                                font.bold: themeDelegate.highlighted
                            }
                            background: Rectangle {
                                color: themeDelegate.highlighted ? Theme.accentSoft : "transparent"
                                radius: Theme.radiusSoft
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.surface
                    border.color: Theme.border
                    radius: Theme.radius

                    GridLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        columns: 2
                        columnSpacing: Theme.gap
                        rowSpacing: 6

                        ListModel {
                            id: swatchModel
                            ListElement {
                                label: "Janela"
                                colorKey: "window"
                            }
                            ListElement {
                                label: "Surface"
                                colorKey: "surface"
                            }
                            ListElement {
                                label: "Panel"
                                colorKey: "panel"
                            }
                            ListElement {
                                label: "Panel Raised"
                                colorKey: "panelRaised"
                            }
                            ListElement {
                                label: "Header"
                                colorKey: "header"
                            }
                            ListElement {
                                label: "Table Header"
                                colorKey: "tableHeader"
                            }
                            ListElement {
                                label: "Border"
                                colorKey: "border"
                            }
                            ListElement {
                                label: "Border Soft"
                                colorKey: "borderSoft"
                            }
                            ListElement {
                                label: "Text"
                                colorKey: "text"
                            }
                            ListElement {
                                label: "Muted Text"
                                colorKey: "mutedText"
                            }
                            ListElement {
                                label: "Accent"
                                colorKey: "accent"
                            }
                            ListElement {
                                label: "Accent Text"
                                colorKey: "accentText"
                            }
                            ListElement {
                                label: "Accent Soft"
                                colorKey: "accentSoft"
                            }
                            ListElement {
                                label: "Accent Strong"
                                colorKey: "accentStrong"
                            }
                            ListElement {
                                label: "Danger"
                                colorKey: "danger"
                            }
                            ListElement {
                                label: "Row Selected"
                                colorKey: "rowSelected"
                            }
                        }

                        Repeater {
                            model: swatchModel

                            delegate: Rectangle {
                                required property string label
                                required property string colorKey
                                readonly property color swatchColor: Theme[colorKey]
                                Layout.fillWidth: true
                                Layout.preferredHeight: 26
                                color: "transparent"

                                Rectangle {
                                    id: swatch
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 22
                                    height: 22
                                    radius: 3
                                    color: parent.swatchColor
                                    border.color: Theme.border
                                    border.width: 1
                                }

                                Label {
                                    anchors.left: swatch.right
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: parent.label
                                    color: Theme.text
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.gap

                    Item {
                        Layout.fillWidth: true
                    }

                    ActionButton {
                        text: "Cancelar"
                        implicitWidth: 110
                        onClicked: root.reject()
                    }
                    ActionButton {
                        text: "OK"
                        implicitWidth: 90
                        onClicked: root.accept()
                    }
                }
            }
        }
    }
}
