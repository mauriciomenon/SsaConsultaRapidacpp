pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    property string density: "normal"
    readonly property int titleTextSize: Theme.densityValue(root.density, 14, 16, 18)
    readonly property int labelTextSize: Theme.densityValue(root.density, 10, 11, 12)
    readonly property int valueTextSize: Theme.densityValue(root.density, 11, 12, 14)
    signal openRequested()

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.cardGap
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: root.viewModel.title
                color: Theme.text
                font.bold: true
                font.pixelSize: root.titleTextSize
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }
            ActionButton {
                text: "Abrir"
                enabled: root.viewModel.fieldCount > 0
                onClicked: root.openRequested()
            }
        }

        ListView {
            id: detailsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.viewModel.fieldCount > 0
            clip: true
            interactive: true
            model: root.viewModel.fields
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Column {
                id: fieldDelegate
                required property string label
                required property var value
                property string rowValue: value === undefined || value === null ? "" : String(value)

                width: detailsList.width
                spacing: 0

                Label {
                    width: parent.width
                    font.pixelSize: root.labelTextSize
                    text: fieldDelegate.label
                    color: Theme.mutedText
                    elide: Text.ElideRight
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.border
                    opacity: 0.5
                }

                Text {
                    width: parent.width
                    text: fieldDelegate.rowValue
                    color: Theme.text
                    wrapMode: Text.Wrap
                    font.pixelSize: root.valueTextSize
                }

                Rectangle {
                    width: parent.width
                    height: Theme.gap
                    color: "transparent"
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.viewModel.fieldCount === 0
            text: "Selecione uma linha da tabela para ver os detalhes"
            color: Theme.mutedText
            font.pixelSize: root.valueTextSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }
}
