import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel

    Layout.preferredHeight: 94
    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            AppCheckBox {
                Layout.preferredWidth: 170
                text: "Excluir SCA/SES/STE"
                checked: root.viewModel.filters.excludeScaSesSte
                onToggled: root.viewModel.filters.excludeScaSesSte = checked
            }
            Label { text: "Executor:"; color: Theme.text }
            TextField {
                Layout.preferredWidth: 130
                implicitHeight: Theme.controlHeight
                text: root.viewModel.filters.quickSector
                onTextChanged: root.viewModel.filters.quickSector = text
                onAccepted: root.viewModel.apply()
            }
            Label { text: "Coluna:"; color: Theme.text }
            ComboBox {
                id: columnBox

                Layout.preferredWidth: 190
                model: root.viewModel.filters.filterColumnKeys
                currentIndex: Math.max(0, root.viewModel.filters.filterColumnKeys.indexOf(root.viewModel.filters.columnKey))
                onActivated: root.viewModel.filters.columnKey = currentText
            }
            TextField {
                Layout.fillWidth: true
                implicitHeight: Theme.controlHeight
                placeholderText: "Filtro da coluna selecionada"
                text: root.viewModel.filters.columnValue
                onTextChanged: root.viewModel.filters.columnValue = text
                onAccepted: root.viewModel.filters.addColumnFilter()
            }
            ActionButton { text: "Adicionar"; onClicked: root.viewModel.filters.addColumnFilter() }
            ActionButton { text: "Padrao"; onClicked: root.viewModel.filters.resetFilters() }
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            text: root.viewModel.filters.activeFilters.join("  |  ")
            color: Theme.mutedText
            elide: Text.ElideRight
        }
    }
}
