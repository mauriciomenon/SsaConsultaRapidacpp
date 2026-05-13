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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: Theme.densityValue(root.density, 6, Theme.gap, Theme.gap)

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: root.viewModel.title
                color: Theme.text
                font.bold: true
                font.pixelSize: root.titleTextSize
                elide: Text.ElideRight
            }
            ActionButton { text: "Abrir"; onClicked: root.openRequested() }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            visible: root.viewModel.fieldCount > 0

            Column {
                width: parent.width
                spacing: Theme.densityValue(root.density, 4, 6, 8)
                Repeater {
                    model: root.viewModel.fields
                    delegate: Column {
                        id: fieldDelegate
                        required property string label
                        required property var value

                        width: parent.width
                        Label {
                            text: fieldDelegate.label
                            color: Theme.mutedText
                            font.pixelSize: root.labelTextSize
                        }
                        Text {
                            width: parent.width
                            text: fieldDelegate.value
                            color: Theme.text
                            wrapMode: Text.Wrap
                            font.pixelSize: root.valueTextSize
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.viewModel.fieldCount === 0
            text: "Selecione uma SSA na tabela"
            color: Theme.mutedText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }
}
