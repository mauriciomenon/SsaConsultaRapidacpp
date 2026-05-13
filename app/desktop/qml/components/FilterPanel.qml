import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel

    signal applyRequested()

    Layout.preferredHeight: 164
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

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label { text: "Semana:"; color: Theme.text }
            ComboBox {
                Layout.preferredWidth: 170
                model: root.filterViewModel.weekColumnKeys
                currentIndex: Math.max(0, root.filterViewModel.weekColumnKeys.indexOf(root.filterViewModel.weekColumnKey))
                onActivated: root.filterViewModel.weekColumnKey = currentText
            }
            TextField {
                Layout.preferredWidth: 80
                implicitHeight: Theme.controlHeight
                placeholderText: "Ano"
                text: root.filterViewModel.yearFilter
                inputMethodHints: Qt.ImhDigitsOnly
                onTextChanged: root.filterViewModel.yearFilter = text
                onAccepted: root.applyRequested()
            }
            TextField {
                Layout.preferredWidth: 90
                implicitHeight: Theme.controlHeight
                placeholderText: "Sem."
                text: root.filterViewModel.weekFilter
                inputMethodHints: Qt.ImhDigitsOnly
                onTextChanged: root.filterViewModel.weekFilter = text
                onAccepted: root.applyRequested()
            }
            ComboBox {
                readonly property var derivationOptions: ["all", "root", "derived"]

                Layout.preferredWidth: 120
                model: derivationOptions
                currentIndex: Math.max(0, derivationOptions.indexOf(root.filterViewModel.derivationMode))
                onActivated: root.filterViewModel.derivationMode = currentText
            }
            AppCheckBox {
                Layout.preferredWidth: 170
                text: "Reprogramadas"
                checked: root.filterViewModel.onlyReprogrammed
                onToggled: root.filterViewModel.onlyReprogrammed = checked
            }
            Item { Layout.fillWidth: true }
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
