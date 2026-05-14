import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    signal applyRequested()
    property var columnValueOptions: root.filterViewModel.columnValueOptions
    property var derivationOptions: root.filterViewModel.derivationModeOptions

    function applyColumnValueFilter() {
        if (!root.filterViewModel.columnValue) {
            return
        }
        root.filterViewModel.addColumnFilter()
        columnValueInput.editText = ""
        columnValueInput.forceActiveFocus()
    }

    Layout.preferredHeight: 182
    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.cardGap
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                text: "Filtros avancados"
                color: Theme.text
                font.bold: true
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }
            Label {
                text: "Clique em Aplicar filtros para atualizar"
                color: Theme.mutedText
                font.pixelSize: 10
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            AppCheckBox {
                Layout.preferredWidth: 170
                text: "Excluir status de exclusao"
                checked: root.filterViewModel.excludeScaSesSte
                onToggled: root.filterViewModel.excludeScaSesSte = checked
            }
            Label { text: "Setor:"; color: Theme.text }
            AppTextField {
                Layout.preferredWidth: 130
                id: quickSectorInput
                focus: true
                text: root.filterViewModel.quickSector
                onTextChanged: root.filterViewModel.quickSector = text
                onAccepted: root.applyRequested()
                onActiveFocusChanged: if (activeFocus) {
                    quickSectorInput.selectAll();
                }
            }
            Label { text: "Coluna:"; color: Theme.text }
            ComboBox {
                id: columnCombo
                Layout.preferredWidth: 190
                model: root.filterViewModel.filterColumnKeys
                currentIndex: Math.max(
                    0,
                    root.filterViewModel.filterColumnKeys.indexOf(root.filterViewModel.columnKey))
                onCurrentTextChanged: {
                    root.filterViewModel.columnKey = currentText
                    root.filterViewModel.refreshColumnValueOptions()
                    columnValueInput.editText = ""
                    root.filterViewModel.columnValue = ""
                    columnValueInput.forceActiveFocus()
                }
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
            ComboBox {
                id: columnValueInput

                Layout.fillWidth: true
                Layout.minimumWidth: 220
                editable: true
                model: root.filterViewModel.columnValueOptions
                onAccepted: root.applyColumnValueFilter()
                onActivated: {
                    if (currentText === "") {
                        return
                    }
                    if (columnValueInput.editText !== currentText) {
                        columnValueInput.editText = currentText
                    }
                    root.filterViewModel.columnValue = currentText
                    columnValueInput.forceActiveFocus()
                }
                onEditTextChanged: root.filterViewModel.columnValue = editText
                Keys.onEscapePressed: root.filterViewModel.columnValue = ""
            }
            ActionButton {
                text: "Adicionar"
                enabled: root.filterViewModel.columnValue.trim() !== ""
                onClicked: {
                    root.applyColumnValueFilter()
                }
            }
            ActionButton {
                text: "Aplicar filtros"
                onClicked: root.applyRequested()
            }
            ActionButton {
                text: "Padrao"
                onClicked: root.filterViewModel.resetFilters()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label { text: "Semana:"; color: Theme.text }
            ComboBox {
                Layout.preferredWidth: 170
                model: root.filterViewModel.weekColumnKeys
                currentIndex: Math.max(
                    0,
                    root.filterViewModel.weekColumnKeys.indexOf(root.filterViewModel.weekColumnKey))
                onActivated: root.filterViewModel.weekColumnKey = currentText
            }
            AppTextField {
                Layout.preferredWidth: 80
                id: yearFilterInput
                placeholderText: "Ano"
                text: root.filterViewModel.yearFilter
                inputMethodHints: Qt.ImhDigitsOnly
                onTextChanged: root.filterViewModel.yearFilter = text
                onAccepted: root.applyRequested()
                Keys.onRightPressed: weekFilterInput.forceActiveFocus()
            }
            AppTextField {
                Layout.preferredWidth: 90
                id: weekFilterInput
                placeholderText: "Sem."
                text: root.filterViewModel.weekFilter
                inputMethodHints: Qt.ImhDigitsOnly
                onTextChanged: root.filterViewModel.weekFilter = text
                onAccepted: root.applyRequested()
                Keys.onLeftPressed: yearFilterInput.forceActiveFocus()
            }

            ComboBox {
                Layout.preferredWidth: 120
                model: root.derivationOptions
                currentIndex: Math.max(
                    0,
                    root.derivationOptions.indexOf(root.filterViewModel.derivationMode))
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
