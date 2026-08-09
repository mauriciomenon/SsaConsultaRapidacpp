pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

Rectangle {
    id: root

    required property var headers
    required property var rows
    property string categoryHeader: qsTr("Categoria")

    readonly property real horizontalPadding: Theme.spacingSm * 2
    readonly property real minimumCategoryColumnWidth: Theme.controlHeight * 4
    readonly property real minimumValueColumnWidth: Theme.controlHeight * 3
    readonly property real maximumColumnWidth: Math.max(Theme.controlHeight * 6, width * 0.55)
    readonly property real categoryColumnWidth: measuredCategoryColumnWidth()
    readonly property var measuredValueColumnWidths: valueColumnWidths()
    readonly property real measuredTableWidth: categoryColumnWidth + measuredValueColumnWidths.reduce((sum, value) => sum + value, 0)
    readonly property real extraValueColumnWidth: headers.length > 0 ? Math.max(0, width - measuredTableWidth) / headers.length : 0
    readonly property real tableWidth: Math.max(width, measuredTableWidth)
    readonly property real headerHeight: Math.max(Theme.controlHeight, tableFontMetrics.height + horizontalPadding)
    readonly property real rowHeight: Math.max(Theme.controlHeight, tableFontMetrics.height + horizontalPadding)

    implicitHeight: Math.min(Theme.controlHeight * 10, tableColumn.implicitHeight + 2)
    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius
    clip: true
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Tabela textual dos dados do grafico")

    FontMetrics {
        id: tableFontMetrics

        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeCaption
    }

    function displayCategory(value) {
        return String(value === null || value === undefined ? "" : value).replace(/\r?\n/g, " / ");
    }

    function measuredCategoryColumnWidth() {
        let measured = tableFontMetrics.advanceWidth(root.categoryHeader);
        for (let index = 0; index < root.rows.length; ++index)
            measured = Math.max(measured, tableFontMetrics.advanceWidth(root.displayCategory(root.rows[index].category)));
        return Math.min(root.maximumColumnWidth, Math.max(root.minimumCategoryColumnWidth, measured + root.horizontalPadding));
    }

    function valueColumnWidths() {
        const widths = [];
        for (let column = 0; column < root.headers.length; ++column) {
            let measured = tableFontMetrics.advanceWidth(String(root.headers[column]));
            for (let rowIndex = 0; rowIndex < root.rows.length; ++rowIndex)
                measured = Math.max(measured, tableFontMetrics.advanceWidth(String(root.rows[rowIndex].values[column] ?? "")));
            widths.push(Math.min(root.maximumColumnWidth, Math.max(root.minimumValueColumnWidth, measured + root.horizontalPadding)));
        }
        return widths;
    }

    function valueColumnWidth(index) {
        return (root.measuredValueColumnWidths[index] ?? root.minimumValueColumnWidth) + root.extraValueColumnWidth;
    }

    Flickable {
        id: tableFlick

        anchors.fill: parent
        anchors.margins: 1
        contentWidth: Math.max(width, root.tableWidth)
        contentHeight: tableColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.horizontal: ScrollBar {
            policy: tableFlick.contentWidth > tableFlick.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        }
        ScrollBar.vertical: ScrollBar {
            policy: tableFlick.contentHeight > tableFlick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        }

        Column {
            id: tableColumn

            width: Math.max(parent.width, root.tableWidth)

            Row {
                Rectangle {
                    width: root.categoryColumnWidth
                    height: root.headerHeight
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
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Repeater {
                    model: root.headers

                    Rectangle {
                        id: headerCell

                        required property int index
                        required property var modelData

                        width: root.valueColumnWidth(headerCell.index)
                        height: root.headerHeight
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
                            font.bold: true
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
                    Accessible.name: root.displayCategory(modelData.category) + ": " + modelData.values.join(", ")

                    Rectangle {
                        width: root.categoryColumnWidth
                        height: root.rowHeight
                        color: dataRow.index % 2 === 0 ? Theme.panel : Theme.rowAlt
                        border.color: Theme.borderSoft

                        Text {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSm
                            text: root.displayCategory(dataRow.modelData.category)
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

                            width: root.valueColumnWidth(valueCell.index)
                            height: root.rowHeight
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
