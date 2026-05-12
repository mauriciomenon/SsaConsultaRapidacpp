import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    signal openRequested()

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: root.viewModel.title
                color: Theme.text
                font.bold: true
                font.pixelSize: 16
                elide: Text.ElideRight
            }
            ActionButton { text: "Abrir"; onClicked: root.openRequested() }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            visible: root.viewModel.fields.length > 0

            Column {
                width: parent.width
                spacing: 6
                Repeater {
                    model: root.viewModel.fields
                    delegate: Column {
                        id: fieldDelegate
                        required property var modelData

                        width: parent.width
                        Label {
                            text: fieldDelegate.modelData.label
                            color: Theme.mutedText
                            font.pixelSize: 11
                        }
                        Text {
                            width: parent.width
                            text: fieldDelegate.modelData.value
                            color: Theme.text
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.viewModel.fields.length === 0
            text: "Selecione uma SSA na tabela"
            color: Theme.mutedText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }
}
