pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var filterViewModel
    required property var derivation
    signal applyRequested

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
        AppComboBox {
            id: sectorSelector
            Layout.fillWidth: true
            model: root.filterViewModel.sector.selectorValues
            currentIndex: root.filterViewModel.sector.selectorIndex
            displayText: currentIndex <= 0 ? "Todos" : currentText
            onActivated: {
                root.filterViewModel.sector.quickSector = sectorSelector.currentText;
                root.applyRequested();
            }
            delegate: ItemDelegate {
                required property string modelData
                width: sectorSelector.width
                text: modelData.length === 0 ? "Todos" : modelData
            }
        }
        AppCheckBox {
            Layout.fillWidth: true
            text: "Excluir SCA/SES/STE"
            checked: root.filterViewModel.sector.excludeScaSesSte
            onToggled: {
                root.filterViewModel.sector.excludeScaSesSte = checked;
                root.applyRequested();
            }
        }
        AppCheckBox {
            Layout.fillWidth: true
            text: "Reprogramadas"
            checked: root.derivation.onlyReprogrammed
            onToggled: {
                root.derivation.onlyReprogrammed = checked;
                root.applyRequested();
            }
        }

        FilterFieldLabel {
            text: "Derivadas"
        }
        AppComboBox {
            Layout.fillWidth: true
            model: root.derivation.derivationModeOptions
            currentIndex: Math.max(0, root.derivation.derivationModeOptions.indexOf(root.derivation.derivationMode))
            onActivated: {
                root.derivation.derivationMode = currentText;
                root.applyRequested();
            }
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
