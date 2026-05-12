pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    readonly property bool compactDensity: root.viewModel.density === "compact"
    readonly property bool comfortableDensity: root.viewModel.density === "comfortable"
    readonly property int headerHeight: compactDensity ? 28 : (comfortableDensity ? 38 : 32)
    readonly property int rowHeight: compactDensity ? 24 : (comfortableDensity ? 36 : 30)
    readonly property int textSize: compactDensity ? 11 : (comfortableDensity ? 13 : 12)

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Flickable {
            id: header
            Layout.fillWidth: true
            Layout.preferredHeight: root.headerHeight
            contentWidth: headerRow.width
            clip: true
            interactive: false

            Row {
                id: headerRow
                height: parent.height

                Repeater {
                    model: root.viewModel.tableModel.visibleColumnCount

                    delegate: Rectangle {
                        required property int index

                        width: root.viewModel.tableModel.columnWidth(index)
                        height: header.height
                        color: Theme.header
                        border.color: Theme.border

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            text: root.viewModel.tableModel.columnLabel(parent.index) +
                                  (root.viewModel.sortColumnKey === root.viewModel.tableModel.columnKey(parent.index)
                                   ? (root.viewModel.sortAscending ? "  ^" : "  v")
                                   : "")
                            color: Theme.text
                            font.bold: true
                            font.pixelSize: root.textSize
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.viewModel.sortByColumn(parent.index)
                        }
                    }
                }
            }
        }

        TableView {
            id: table
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.viewModel.tableModel
            clip: true
            rowSpacing: 1
            columnSpacing: 1
            boundsBehavior: Flickable.StopAtBounds
            onContentXChanged: header.contentX = contentX

            delegate: Rectangle {
                required property int row
                required property int column
                required property string display

                implicitWidth: root.viewModel.tableModel.columnWidth(column)
                implicitHeight: root.rowHeight
                color: row % 2 === 0 ? Theme.panel : Theme.rowAlt
                border.color: "#dde4ec"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    text: parent.display
                    color: Theme.text
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: root.textSize
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.viewModel.selectRow(parent.row)
                    onDoubleClicked: root.viewModel.openSelectedSsa()
                }
            }
        }
    }
}
