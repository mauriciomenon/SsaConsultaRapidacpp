pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var filterViewModel
    required property var derivation
    signal applyRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: 128

    GridLayout {
        anchors.fill: parent
        columns: 4
        columnSpacing: Theme.gap
        rowSpacing: 6

        FilterFieldLabel {
            text: "Setor Executor"
        }
        AppTextField {
            Layout.fillWidth: true
            text: root.filterViewModel.sector.quickSector
            placeholderText: "Todos"
            onTextEdited: root.filterViewModel.sector.quickSector = text
            onAccepted: root.applyRequested()
        }
        AppCheckBox {
            Layout.fillWidth: true
            text: "Excluir SCA/SES/STE"
            checked: root.filterViewModel.sector.excludeScaSesSte
            onToggled: root.filterViewModel.sector.excludeScaSesSte = checked
        }
        AppCheckBox {
            Layout.fillWidth: true
            text: "Reprogramadas"
            checked: root.derivation.onlyReprogrammed
            onToggled: root.derivation.onlyReprogrammed = checked
        }

        FilterFieldLabel {
            text: "Derivadas"
        }
        AppComboBox {
            Layout.fillWidth: true
            model: root.derivation.derivationModeOptions
            currentIndex: Math.max(
                0,
                root.derivation.derivationModeOptions.indexOf(root.derivation.derivationMode))
            onActivated: root.derivation.derivationMode = currentText
        }
        FilterFieldLabel {
            text: "Reprogramacoes ="
        }
        AppTextField {
            Layout.fillWidth: true
            text: root.derivation.reprogrammingEqualsFilter
            placeholderText: "0, 1, 2..."
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.derivation.reprogrammingEqualsFilter = text
            onAccepted: root.applyRequested()
        }
    }
}
