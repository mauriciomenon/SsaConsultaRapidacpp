pragma ComponentBehavior: Bound

import QtQuick
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    TableView {
        id: table
        anchors.fill: parent
        anchors.margins: 1
        model: root.viewModel.tableModel
        clip: true
        rowSpacing: 1
        columnSpacing: 1
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            required property int row
            required property int column
            required property string display

            implicitWidth: column === 9 || column === 10 ? 320 : 132
            implicitHeight: 30
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
                font.pixelSize: 12
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.viewModel.selectRow(parent.row)
                onDoubleClicked: root.viewModel.openSelectedSsa()
            }
        }
    }
}
