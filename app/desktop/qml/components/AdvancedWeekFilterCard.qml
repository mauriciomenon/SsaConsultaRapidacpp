pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var week
    required property var advanced
    signal applyRequested

    Layout.fillWidth: true
    Layout.preferredHeight: 126

    GridLayout {
        anchors.fill: parent
        columns: 6
        columnSpacing: Theme.gap
        rowSpacing: 6

        FilterFieldLabel {
            text: "Semana generica"
        }
        AppComboBox {
            Layout.fillWidth: true
            model: root.week.weekColumnKeys
            currentIndex: Math.max(0, root.week.weekColumnKeys.indexOf(root.week.weekColumnKey))
            onActivated: root.week.weekColumnKey = currentText
        }
        AppTextField {
            Layout.preferredWidth: 80
            placeholderText: "Ano"
            text: root.week.yearFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.yearFilter = text
            onAccepted: root.applyRequested()
        }
        AppTextField {
            Layout.preferredWidth: 80
            placeholderText: "Sem."
            text: root.week.weekFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.weekFilter = text
            onAccepted: root.applyRequested()
        }
        AppTextField {
            Layout.preferredWidth: 96
            placeholderText: "Ano Emis."
            text: root.week.issueYearFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.issueYearFilter = text
            onAccepted: root.applyRequested()
        }
        AppTextField {
            Layout.preferredWidth: 96
            placeholderText: "Ano Exec."
            text: root.week.executionYearFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.executionYearFilter = text
            onAccepted: root.applyRequested()
        }

        FilterFieldLabel {
            text: "AnoSemana"
        }
        AppTextField {
            Layout.fillWidth: true
            placeholderText: "Emissao inicio"
            text: root.week.issueWeekStartFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.issueWeekStartFilter = text
            onAccepted: root.applyRequested()
        }
        AppTextField {
            Layout.fillWidth: true
            placeholderText: "Emissao fim"
            text: root.week.issueWeekEndFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.issueWeekEndFilter = text
            onAccepted: root.applyRequested()
        }
        AppTextField {
            Layout.fillWidth: true
            placeholderText: "Execucao inicio"
            text: root.week.executionWeekStartFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.executionWeekStartFilter = text
            onAccepted: root.applyRequested()
        }
        AppTextField {
            Layout.fillWidth: true
            placeholderText: "Execucao fim"
            text: root.week.executionWeekEndFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.executionWeekEndFilter = text
            onAccepted: root.applyRequested()
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton {
                text: "Aplicar"
                implicitWidth: 90
                onClicked: root.applyRequested()
            }
            ActionButton {
                text: "Limpar avancados"
                implicitWidth: 140
                onClicked: root.advanced.clear()
            }
        }
    }
}
