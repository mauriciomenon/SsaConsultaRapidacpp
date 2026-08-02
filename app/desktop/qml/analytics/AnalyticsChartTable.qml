pragma ComponentBehavior: Bound

import QtQuick
import SsaConsultaRapida

Rectangle {
    id: root

    required property var headers
    required property var rows
    property string categoryHeader: qsTr("Categoria")

    readonly property real categoryColumnWidth: 132
    readonly property real valueColumnWidth: Math.max(108, (width - categoryColumnWidth) / Math.max(1, headers.length))
    readonly property real tableWidth: categoryColumnWidth + headers.length * valueColumnWidth

    implicitHeight: Math.min(220, tableColumn.implicitHeight + 2)
    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius
    clip: true
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Tabela textual dos dados do grafico")

    Flickable {
        anchors.fill: parent
        anchors.margins: 1
        contentWidth: Math.max(width, root.tableWidth)
        contentHeight: tableColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: tableColumn

            width: Math.max(parent.width, root.tableWidth)

            Row {
                Rectangle {
                    width: root.categoryColumnWidth
                    height: 30
                    color: Theme.tableHeader
                    border.color: Theme.borderSoft

                    Text {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSm
                        text: root.categoryHeader
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        font.bold: false
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Repeater {
                    model: root.headers

                    Rectangle {
                        id: headerCell

                        required property var modelData

                        width: root.valueColumnWidth
                        height: 30
                        color: Theme.tableHeader
                        border.color: Theme.borderSoft

                        Text {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSm
                            text: String(headerCell.modelData)
                            textFormat: Text.PlainText
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: false
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Repeater {
                model: root.rows

                Row {
                    id: dataRow

                    required property int index
                    required property var modelData

                    Accessible.role: Accessible.StaticText
                    Accessible.name: modelData.category + ": " + modelData.values.join(", ")

                    Rectangle {
                        width: root.categoryColumnWidth
                        height: 28
                        color: dataRow.index % 2 === 0 ? Theme.panel : Theme.rowAlt
                        border.color: Theme.borderSoft

                        Text {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSm
                            text: dataRow.modelData.category
                            textFormat: Text.PlainText
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }

                    Repeater {
                        model: root.headers.length

                        Rectangle {
                            id: valueCell

                            required property int index

                            width: root.valueColumnWidth
                            height: 28
                            color: dataRow.index % 2 === 0 ? Theme.panel : Theme.rowAlt
                            border.color: Theme.borderSoft

                            Text {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingSm
                                text: String(dataRow.modelData.values[valueCell.index] ?? "")
                                textFormat: Text.PlainText
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeCaption
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
