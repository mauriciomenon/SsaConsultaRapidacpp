import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel

    Layout.preferredHeight: 86
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

            CheckBox {
                text: "Excluir SCA/SES/STE"
                checked: root.viewModel.filters.excludeClosedStatuses
                onToggled: root.viewModel.filters.excludeClosedStatuses = checked
            }
            Label { text: "Executor:"; color: Theme.text }
            TextField {
                Layout.preferredWidth: 120
                implicitHeight: Theme.controlHeight
                text: root.viewModel.filters.quickSector
                onTextChanged: root.viewModel.filters.quickSector = text
                onAccepted: root.viewModel.apply()
            }
            Label { text: "Coluna:"; color: Theme.text }
            ComboBox {
                id: columnBox
                Layout.preferredWidth: 170
                model: ["situacao", "setor_executor", "setor_emissor", "localizacao_codigo", "descricao_ssa", "derivada_de"]
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
            ActionButton { text: "Limpar"; onClicked: root.viewModel.filters.clearFilters() }
        }

        Label {
            Layout.fillWidth: true
            text: root.viewModel.filters.activeFilters.join("  |  ")
            color: Theme.mutedText
            elide: Text.ElideRight
        }
    }
}
