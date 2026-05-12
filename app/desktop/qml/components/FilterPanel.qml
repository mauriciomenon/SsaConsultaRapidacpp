import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel

    signal applyRequested()

    Layout.preferredHeight: 124
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
                checked: root.filterViewModel.excludeScaSesSte
                onToggled: root.filterViewModel.excludeScaSesSte = checked
            }
            Label { text: "Executor:"; color: Theme.text }
            TextField {
                Layout.preferredWidth: 130
                implicitHeight: Theme.controlHeight
                text: root.filterViewModel.quickSector
                onTextChanged: root.filterViewModel.quickSector = text
                onAccepted: root.applyRequested()
            }
            Label { text: "Coluna:"; color: Theme.text }
            ComboBox {
                id: columnBox

                Layout.preferredWidth: 190
                model: root.filterViewModel.filterColumnKeys
                currentIndex: Math.max(0, root.filterViewModel.filterColumnKeys.indexOf(root.filterViewModel.columnKey))
                onActivated: root.filterViewModel.columnKey = currentText
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                text: "Filtro:"
                color: Theme.text
            }
            TextField {
                Layout.fillWidth: true
                Layout.minimumWidth: 220
                implicitHeight: Theme.controlHeight
                placeholderText: "Filtro da coluna selecionada"
                text: root.filterViewModel.columnValue
                onTextChanged: root.filterViewModel.columnValue = text
                onAccepted: root.filterViewModel.addColumnFilter()
            }
            ActionButton { text: "Adicionar"; onClicked: root.filterViewModel.addColumnFilter() }
            ActionButton { text: "Padrao"; onClicked: root.filterViewModel.resetFilters() }
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            text: root.filterViewModel.activeFilterSummary
            color: Theme.mutedText
            elide: Text.ElideRight
        }
    }
}
