import QtQuick
import QtQuick.Layouts
import SsaConsultaRapida

Window {
    id: root

    property string selectedTheme: "ayu-light"
    readonly property string renderedTheme: Theme.themeName
    readonly property bool wide: width >= 840
    readonly property var swatches: [
        {
            label: qsTr("Accent"),
            value: Theme.accent
        },
        {
            label: qsTr("Soft"),
            value: Theme.accentSoft
        },
        {
            label: qsTr("Strong"),
            value: Theme.accentStrong
        },
        {
            label: qsTr("Danger"),
            value: Theme.danger
        }
    ]

    width: 960
    height: 720
    visible: true
    color: Theme.window
    title: qsTr("Theme gallery")

    Binding {
        target: Theme
        property: "themeName"
        value: root.selectedTheme
    }

    Item {
        id: galleryContent

        objectName: "galleryContent"
        anchors.fill: parent
        anchors.margins: 16

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.cardGap

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: Theme.header
                border.color: Theme.border
                radius: Theme.radiusSoft

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: Theme.gap

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("SSA Consulta Rapida")
                            textFormat: Text.PlainText
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeHeader
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Native palette preview")
                            textFormat: Text.PlainText
                            color: Theme.mutedText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        implicitWidth: themeLabel.implicitWidth + 20
                        implicitHeight: 28
                        color: Theme.accentSoft
                        border.color: Theme.accent
                        radius: Theme.radius

                        Text {
                            id: themeLabel

                            anchors.centerIn: parent
                            text: root.renderedTheme
                            textFormat: Text.PlainText
                            color: Theme.readableText(parent.color)
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: root.wide ? 2 : 1
                rowSpacing: Theme.cardGap
                columnSpacing: Theme.cardGap

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 230
                    color: Theme.panel
                    border.color: Theme.border
                    radius: Theme.radiusSoft

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: Theme.gap

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Search controls")
                            textFormat: Text.PlainText
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                            font.bold: true
                        }

                        AppTextField {
                            Layout.fillWidth: true
                            text: "!G097"
                            placeholderText: qsTr("Search SSA")
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.gap

                            AppComboBox {
                                Layout.fillWidth: true
                                model: [qsTr("All sectors"), "SEE", "MEL4"]
                                currentIndex: 1
                            }

                            AppCheckBox {
                                Layout.preferredWidth: 120
                                text: qsTr("Include closed")
                                checked: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.gap

                            ActionButton {
                                Layout.fillWidth: true
                                text: qsTr("Apply")
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: qsTr("Clear")
                                danger: true
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: qsTr("Disabled")
                                enabled: false
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm

                            Repeater {
                                model: root.swatches

                                Rectangle {
                                    id: swatchDelegate

                                    required property var modelData

                                    Layout.fillWidth: true
                                    implicitHeight: 38
                                    color: modelData.value
                                    border.color: Theme.border
                                    radius: Theme.radius

                                    Text {
                                        anchors.centerIn: parent
                                        text: swatchDelegate.modelData.label
                                        textFormat: Text.PlainText
                                        color: Theme.readableText(parent.color)
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCaption
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 230
                    color: Theme.surface
                    border.color: Theme.border
                    radius: Theme.radiusSoft

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 0

                        Text {
                            Layout.fillWidth: true
                            Layout.bottomMargin: Theme.gap
                            text: qsTr("Results")
                            textFormat: Text.PlainText
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 34
                            color: Theme.tableHeader
                            border.color: Theme.border

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10

                                Text {
                                    Layout.preferredWidth: 90
                                    text: qsTr("Number")
                                    textFormat: Text.PlainText
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeMicro
                                    font.bold: true
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Description")
                                    textFormat: Text.PlainText
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeMicro
                                    font.bold: true
                                }

                                Text {
                                    Layout.preferredWidth: 72
                                    text: qsTr("Status")
                                    textFormat: Text.PlainText
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeMicro
                                    font.bold: true
                                }
                            }
                        }

                        Repeater {
                            model: [
                                {
                                    number: "2026001234",
                                    description: qsTr("Scheduled maintenance"),
                                    status: qsTr("Open")
                                },
                                {
                                    number: "2026001235",
                                    description: qsTr("Protection inspection"),
                                    status: qsTr("Pending")
                                },
                                {
                                    number: "2026001236",
                                    description: qsTr("Network adjustment"),
                                    status: qsTr("Closed")
                                },
                                {
                                    number: "2026001237",
                                    description: qsTr("Equipment replacement"),
                                    status: qsTr("Open")
                                }
                            ]

                            Rectangle {
                                id: resultDelegate

                                required property int index
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: 42
                                color: index === 1 ? Theme.rowSelected : index % 2 === 0 ? Theme.panel : Theme.rowAlt
                                border.color: Theme.borderSoft

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10

                                    Text {
                                        Layout.preferredWidth: 90
                                        text: resultDelegate.modelData.number
                                        textFormat: Text.PlainText
                                        color: Theme.text
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: resultDelegate.modelData.description
                                        textFormat: Text.PlainText
                                        color: Theme.text
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.preferredWidth: 72
                                        text: resultDelegate.modelData.status
                                        textFormat: Text.PlainText
                                        color: resultDelegate.index === 1 ? Theme.accentStrong : Theme.mutedText
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMicro
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 38
                            color: Theme.panelRaised
                            border.color: Theme.borderSoft

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("4 records - page 1 of 1")
                                textFormat: Text.PlainText
                                color: Theme.mutedText
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeCaption
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: Theme.panelRaised
                border.color: Theme.border
                radius: Theme.radiusSoft

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: Theme.gap

                    Rectangle {
                        implicitWidth: 10
                        implicitHeight: 10
                        radius: 5
                        color: Theme.accent
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Ready - database loaded")
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMicro
                        elide: Text.ElideRight
                    }

                    Text {
                        text: qsTr("Connection healthy")
                        textFormat: Text.PlainText
                        color: Theme.link
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMicro
                    }
                }
            }
        }
    }
}
