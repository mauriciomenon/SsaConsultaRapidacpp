pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    required property string density
    signal openRequested()

    readonly property int headerHeight: Theme.densityValue(root.density, 28, 32, 38)
    readonly property int rowHeight: Theme.densityValue(root.density, 24, 30, 36)
    readonly property int textSize: Theme.densityValue(root.density, 11, 12, 13)
    readonly property var tableColumns: root.viewModel.tableHeaders

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
                spacing: 1

                Repeater {
                    model: root.tableColumns

                    delegate: Rectangle {
                        required property int index
                        required property var modelData

                        width: root.viewModel.tableModel.columnWidth(index)
                        height: header.height
                        color: Theme.header
                        border.color: Theme.border
                        border.width: 0

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.topMargin: 4
                            anchors.bottomMargin: 4
                            text: parent.modelData.label
                            color: Theme.text
                            font.bold: true
                            font.pixelSize: root.textSize
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    root.viewModel.setFilterColumn(parent.modelData.key)
                                    return
                                }
                                root.viewModel.sortByColumn(parent.index)
                            }
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
            columnWidthProvider: function(column) {
                return root.viewModel.tableModel.columnWidth(column)
            }
            rowHeightProvider: function(row) {
                return table.cachedRowHeight
            }
            readonly property int cachedRowHeight: root.rowHeight
            readonly property int cachedTextSize: root.textSize
            onContentXChanged: header.contentX = contentX
            ScrollBar.horizontal: ScrollBar {}
            ScrollBar.vertical: ScrollBar {}

            Timer {
                id: relayoutTimer
                interval: 0
                repeat: false
                onTriggered: table.forceLayout()
            }

            Connections {
                target: root.viewModel.tableModel

                function onColumnsChanged() {
                    relayoutTimer.restart()
                }
            }

            Connections {
                target: root

                function onDensityChanged() {
                    relayoutTimer.restart()
                }
            }

            delegate: Rectangle {
                required property int row
                required property int column
                required property var displayValue
                readonly property bool isStriped: (row % 2) !== 0

                implicitHeight: table.cachedRowHeight
                color: isStriped ? Theme.rowAlt : Theme.panel
                border.width: 0

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    text: parent.displayValue === "" ? "-" : parent.displayValue
                    color: Theme.text
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: table.cachedTextSize
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.viewModel.selectRow(parent.row)
                    onDoubleClicked: root.openRequested()
                }
            }
        }
    }
}
