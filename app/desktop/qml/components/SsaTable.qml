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

    readonly property int headerHeight: Theme.densityValue(root.density, 26, 30, 36)
    readonly property int rowHeight: Theme.densityValue(root.density, 25, 30, 35)
    readonly property int textSize: Theme.densityValue(root.density, 12, 13, 14)
    readonly property var tableColumns: root.viewModel.tableHeaders

    color: Theme.surface
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

                        width: modelData.width
                        height: header.height
                        color: Theme.tableHeader
                        border.color: Theme.borderSoft
                        border.width: 1

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.topMargin: 4
                            anchors.bottomMargin: 4
                            text: parent.modelData.label
                                  + (parent.modelData.filtered ? " [f]" : "")
                                  + (parent.modelData.sorted
                                     ? (parent.modelData.sortAscending ? "  ^" : "  v")
                                     : "")
                            color: Theme.accentStrong
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
                                    root.viewModel.setFilterPanelFocusColumn(parent.modelData.key)
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
            rowSpacing: 0
            columnSpacing: 1
            boundsBehavior: Flickable.StopAtBounds
            columnWidthProvider: function(column) {
                return column >= 0 && column < root.tableColumns.length
                       ? root.tableColumns[column].width
                       : 0
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
                interval: 33
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
                readonly property var columnConfig: root.tableColumns[column]
                                                    ? root.tableColumns[column]
                                                    : ({})
                readonly property bool isStriped: (row % 2) !== 0
                readonly property bool opensSam: columnConfig.opensSam === true

                implicitHeight: table.cachedRowHeight
                color: isStriped ? Theme.rowAlt : Theme.surface
                border.width: 0

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    text: parent.displayValue === "" ? "-" : parent.displayValue
                    color: parent.opensSam ? Theme.link : Theme.text
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: table.cachedTextSize
                    font.bold: parent.opensSam
                    font.underline: parent.opensSam
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: parent.opensSam ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        root.viewModel.selectRow(parent.row)
                        if (parent.opensSam) {
                            root.openRequested()
                        }
                    }
                }
            }
        }
    }
}
