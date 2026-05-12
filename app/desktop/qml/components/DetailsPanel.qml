pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    property string density: "normal"
    readonly property bool compactDensity: density === "compact"
    readonly property bool comfortableDensity: density === "comfortable"
    readonly property int titleTextSize: compactDensity ? 14 : (comfortableDensity ? 18 : 16)
    readonly property int labelTextSize: compactDensity ? 10 : (comfortableDensity ? 12 : 11)
    readonly property int valueTextSize: compactDensity ? 11 : (comfortableDensity ? 14 : 12)
    signal openRequested()

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: root.compactDensity ? 6 : Theme.gap

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
            visible: root.viewModel.fields.length > 0

            Column {
                width: parent.width
                spacing: root.compactDensity ? 4 : (root.comfortableDensity ? 8 : 6)
                Repeater {
                    model: root.viewModel.fields
                    delegate: Column {
                        id: fieldDelegate
                        required property var modelData

                        width: parent.width
                        Label {
                            text: fieldDelegate.modelData.label
                            color: Theme.mutedText
                            font.pixelSize: root.labelTextSize
                        }
                        Text {
                            width: parent.width
                            text: fieldDelegate.modelData.value
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
            visible: root.viewModel.fields.length === 0
            text: "Selecione uma SSA na tabela"
            color: Theme.mutedText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }
}
