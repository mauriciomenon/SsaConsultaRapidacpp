pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var sectorHierarchy
    required property var macro
    signal applyRequested

    Layout.fillWidth: true
    Layout.preferredHeight: 122

    function divisionIndex() {
        for (var index = 0; index < root.sectorHierarchy.divisions.length; ++index) {
            if (root.sectorHierarchy.divisions[index].key === root.sectorHierarchy.selectedDivision)
                return index;
        }
        return -1;
    }

    function macroIndex() {
        for (var index = 0; index < root.macro.options.length; ++index) {
            if (root.macro.options[index].value === root.macro.selectedMacro)
                return index;
        }
        return 0;
    }

    GridLayout {
        anchors.fill: parent
        columns: 4
        columnSpacing: Theme.gap
        rowSpacing: 6

        FilterFieldLabel {
            text: "Divisao"
        }
        AppComboBox {
            id: divisionSelector
            Layout.fillWidth: true
            textRole: "key"
            valueRole: "key"
            model: root.sectorHierarchy.divisions
            currentIndex: root.divisionIndex()
            displayText: currentIndex >= 0 ? currentText : "Selecionar"
            onActivated: {
                root.sectorHierarchy.applyDivision(currentValue);
                root.applyRequested();
            }
        }
        ActionButton {
            text: "Limpar divisao"
            implicitWidth: 120
            onClicked: {
                root.sectorHierarchy.clearDivision();
                root.applyRequested();
            }
        }
        Label {
            Layout.fillWidth: true
            text: root.sectorHierarchy.selectedDivision.length > 0 ? root.sectorHierarchy.selectedDivision : "Sem divisao"
            color: root.sectorHierarchy.selectedDivision.length > 0 ? Theme.accentStrong : Theme.mutedText
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        FilterFieldLabel {
            text: "Macro"
        }
        AppComboBox {
            id: macroSelector
            Layout.fillWidth: true
            textRole: "label"
            valueRole: "value"
            model: root.macro.options
            currentIndex: root.macroIndex()
            onActivated: {
                root.macro.selectedMacro = currentValue;
                root.applyRequested();
            }
        }
        Label {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: root.macro.reportTitle.length > 0 ? root.macro.reportTitle + ": " + root.macro.reportText : "Sem relatorio macro"
            color: root.macro.reportTitle.length > 0 ? Theme.accentStrong : Theme.mutedText
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
}
