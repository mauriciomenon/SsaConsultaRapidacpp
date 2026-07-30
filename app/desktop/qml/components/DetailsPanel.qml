pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    property string density: "normal"
    readonly property int labelTextSize: Theme.densityValue(root.density, 12, 13, 14)
    readonly property int valueTextSize: Theme.densityValue(root.density, 12, 13, 15)
    readonly property int detailsLabelWidth: Theme.densityValue(root.density, 104, 116, 132)
    signal graphWindowRequested
    // Emitted when the user clicks a relation node: the main view loads that
    // SSA into the details panel (not open SAM).
    signal loadRelationRequested(int relationIndex)

    function isLongField(key) {
        return key === "descricao_ssa" || key === "descricao_execucao" || key === "justificativa" || key === "parciais";
    }

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 3

        DetailsRelationNavigator {
            Layout.fillWidth: true
            viewModel: root.viewModel
            density: root.density
            onGraphWindowRequested: root.graphWindowRequested()
            onLoadRelationRequested: relationIndex => root.loadRelationRequested(relationIndex)
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
                required property string key
                required property string label
                required property var value
                property string rowValue: value === undefined || value === null ? "" : String(value)
                property bool longField: root.isLongField(key)

                width: detailsList.width
                spacing: 0

                RowLayout {
                    width: parent.width
                    spacing: 5

                    Label {
                        Layout.preferredWidth: root.detailsLabelWidth
                        Layout.alignment: Qt.AlignTop
                        font.pixelSize: root.labelTextSize
                        font.bold: false
                        text: fieldDelegate.label + ":"
                        color: Theme.text
                        elide: Text.ElideRight
                    }

                    TextEdit {
                        Layout.fillWidth: true
                        Layout.preferredHeight: fieldDelegate.longField ? Math.max(root.valueTextSize + 5, contentHeight) : root.valueTextSize + 5
                        Layout.maximumHeight: fieldDelegate.longField ? 10000 : root.valueTextSize + 5
                        text: fieldDelegate.rowValue
                        textFormat: TextEdit.PlainText
                        color: Theme.text
                        readOnly: true
                        selectByMouse: true
                        selectedTextColor: Theme.accentText
                        selectionColor: Theme.accent
                        wrapMode: TextEdit.Wrap
                        font.pixelSize: root.valueTextSize
                        font.bold: false
                        clip: true
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.border
                    opacity: 0.8
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
