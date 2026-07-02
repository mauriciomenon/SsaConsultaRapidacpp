pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var week
    required property var advanced
    signal applyRequested

    Layout.fillWidth: true
    Layout.preferredHeight: 88

    function fieldBorder(valid, control) {
        if (!valid)
            return Theme.danger;
        return control.activeFocus ? Theme.accent : Theme.border;
    }

    GridLayout {
        anchors.fill: parent
        columns: 6
        columnSpacing: Theme.gap
        rowSpacing: 4

        FilterFieldLabel {
            text: "Semana"
        }
        AppComboBox {
            Layout.fillWidth: true
            model: root.week.weekColumnKeys
            currentIndex: Math.max(0, root.week.weekColumnKeys.indexOf(root.week.weekColumnKey))
            onActivated: root.week.weekColumnKey = currentText
        }
        AppTextField {
            id: genericYearField
            Layout.preferredWidth: 80
            placeholderText: "Ano"
            text: root.week.yearFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.yearFilter = text
            onAccepted: root.applyRequested()
            background: Rectangle {
                color: Theme.panelRaised
                border.color: root.fieldBorder(root.week.isYearValid(genericYearField.text), genericYearField)
                border.width: 1
                radius: Theme.radius
            }
        }
        AppTextField {
            id: genericWeekField
            Layout.preferredWidth: 80
            placeholderText: "Sem."
            text: root.week.weekFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.weekFilter = text
            onAccepted: root.applyRequested()
            background: Rectangle {
                color: Theme.panelRaised
                border.color: root.fieldBorder(root.week.isWeekValid(genericWeekField.text), genericWeekField)
                border.width: 1
                radius: Theme.radius
            }
        }
        AppTextField {
            id: issueYearField
            Layout.preferredWidth: 96
            placeholderText: "Ano Emis."
            text: root.week.issueYearFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.issueYearFilter = text
            onAccepted: root.applyRequested()
            background: Rectangle {
                color: Theme.panelRaised
                border.color: root.fieldBorder(root.week.isYearValid(issueYearField.text), issueYearField)
                border.width: 1
                radius: Theme.radius
            }
        }
        AppTextField {
            id: executionYearField
            Layout.preferredWidth: 96
            placeholderText: "Ano Exec."
            text: root.week.executionYearFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.executionYearFilter = text
            onAccepted: root.applyRequested()
            background: Rectangle {
                color: Theme.panelRaised
                border.color: root.fieldBorder(root.week.isYearValid(executionYearField.text), executionYearField)
                border.width: 1
                radius: Theme.radius
            }
        }

        FilterFieldLabel {
            text: "Intervalo"
        }
        AppTextField {
            id: issueWeekStartField
            Layout.fillWidth: true
            placeholderText: "Emissao inicio"
            text: root.week.issueWeekStartFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.issueWeekStartFilter = text
            onAccepted: root.applyRequested()
            background: Rectangle {
                color: Theme.panelRaised
                border.color: root.fieldBorder(root.week.isYearWeekValid(issueWeekStartField.text), issueWeekStartField)
                border.width: 1
                radius: Theme.radius
            }
        }
        AppTextField {
            id: issueWeekEndField
            Layout.fillWidth: true
            placeholderText: "Emissao fim"
            text: root.week.issueWeekEndFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.issueWeekEndFilter = text
            onAccepted: root.applyRequested()
            background: Rectangle {
                color: Theme.panelRaised
                border.color: root.fieldBorder(root.week.isYearWeekValid(issueWeekEndField.text), issueWeekEndField)
                border.width: 1
                radius: Theme.radius
            }
        }
        AppTextField {
            id: executionWeekStartField
            Layout.fillWidth: true
            placeholderText: "Execucao inicio"
            text: root.week.executionWeekStartFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.executionWeekStartFilter = text
            onAccepted: root.applyRequested()
            background: Rectangle {
                color: Theme.panelRaised
                border.color: root.fieldBorder(root.week.isYearWeekValid(executionWeekStartField.text), executionWeekStartField)
                border.width: 1
                radius: Theme.radius
            }
        }
        AppTextField {
            id: executionWeekEndField
            Layout.fillWidth: true
            placeholderText: "Execucao fim"
            text: root.week.executionWeekEndFilter
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.week.executionWeekEndFilter = text
            onAccepted: root.applyRequested()
            background: Rectangle {
                color: Theme.panelRaised
                border.color: root.fieldBorder(root.week.isYearWeekValid(executionWeekEndField.text), executionWeekEndField)
                border.width: 1
                radius: Theme.radius
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton {
                text: "Aplicar"
                implicitWidth: 78
                onClicked: root.applyRequested()
            }
            ActionButton {
                text: "Limpar"
                implicitWidth: 70
                ToolTip.visible: hovered
                ToolTip.text: "Limpar filtros avancados"
                ToolTip.delay: 0
                onClicked: root.advanced.clear()
            }
        }
    }
}
