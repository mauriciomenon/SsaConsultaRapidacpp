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
    Layout.preferredHeight: intervalVisible ? 66 : 36
    padding: 2
    color: "transparent"
    border.color: "transparent"
    property bool intervalExpanded: false
    readonly property bool intervalActive: week.issueWeekStartFilter.length > 0 || week.issueWeekEndFilter.length > 0 || week.executionWeekStartFilter.length > 0 || week.executionWeekEndFilter.length > 0
    readonly property bool intervalVisible: intervalExpanded || intervalActive

    function fieldBorder(valid, control) {
        if (!valid)
            return Theme.danger;
        return control.activeFocus ? Theme.accent : Theme.border;
    }

    GridLayout {
        anchors.fill: parent
        columns: root.intervalVisible ? 6 : 8
        columnSpacing: 6
        rowSpacing: 0

        FilterFieldLabel {
            text: "Semana"
            Layout.preferredWidth: 58
            Layout.preferredHeight: Theme.filterRowHeight
        }
        AppComboBox {
            Layout.preferredWidth: Math.min(360, Math.max(220, root.width * 0.28))
            Layout.preferredHeight: Theme.filterRowHeight
            model: root.week.weekColumnKeys
            currentIndex: Math.max(0, root.week.weekColumnKeys.indexOf(root.week.weekColumnKey))
            onActivated: root.week.weekColumnKey = currentText
        }
        AppTextField {
            id: genericYearField
            Layout.preferredWidth: Theme.weekFieldWidth
            Layout.preferredHeight: Theme.filterRowHeight
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
            Layout.preferredWidth: Theme.weekFieldWidth
            Layout.preferredHeight: Theme.filterRowHeight
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
            Layout.preferredWidth: 90
            Layout.preferredHeight: Theme.filterRowHeight
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
            Layout.preferredWidth: 90
            Layout.preferredHeight: Theme.filterRowHeight
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
        ActionButton {
            text: "Int."
            implicitWidth: 42
            implicitHeight: Theme.filterRowHeight
            padding: 0
            font.pixelSize: Theme.fontSizeMicro
            ToolTip.visible: hovered
            ToolTip.text: "Mostrar filtros de intervalo AnoSemana"
            ToolTip.delay: 0
            onClicked: root.intervalExpanded = !root.intervalExpanded
        }

        FilterFieldLabel {
            text: "Intervalo"
            Layout.preferredWidth: 58
            Layout.preferredHeight: Theme.filterRowHeight
            visible: root.intervalVisible
        }
        AppTextField {
            id: issueWeekStartField
            Layout.preferredWidth: 122
            Layout.preferredHeight: Theme.filterRowHeight
            visible: root.intervalVisible
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
            Layout.preferredWidth: 122
            Layout.preferredHeight: Theme.filterRowHeight
            visible: root.intervalVisible
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
            Layout.preferredWidth: 122
            Layout.preferredHeight: Theme.filterRowHeight
            visible: root.intervalVisible
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
            Layout.preferredWidth: 122
            Layout.preferredHeight: Theme.filterRowHeight
            visible: root.intervalVisible
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
            Layout.preferredHeight: Theme.filterRowHeight
            ActionButton {
                text: "Aplicar"
                implicitWidth: 66
                implicitHeight: Theme.filterRowHeight
                padding: 0
                onClicked: root.applyRequested()
            }
            ActionButton {
                text: "Limpar"
                implicitWidth: 58
                implicitHeight: Theme.filterRowHeight
                padding: 0
                ToolTip.visible: hovered
                ToolTip.text: "Limpar filtros avancados"
                ToolTip.delay: 0
                onClicked: root.advanced.clear()
            }
        }
    }
}
