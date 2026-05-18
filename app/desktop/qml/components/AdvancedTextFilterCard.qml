pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var row
    required property var operatorModes
    required property var visibleValues
    required property bool hasMoreValues
    required property bool valuesLoading
    required property string textFilter
    required property int operatorIndex
    required property string operatorLabel
    required property real cardWidth
    required property real cardHeight
    property bool expandedValues: false
    readonly property int compactValueLimit: 18

    signal optionsRequested()
    signal operatorModeRequested(string mode)
    signal selectedValueRequested(string value)
    signal loadedValuesFilterRequested(string mode)
    signal textFilterClearRequested()

    width: cardWidth
    height: cardHeight

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.row.label
                color: Theme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(180, Math.max(72, root.cardWidth * 0.34))
                text: root.textFilter.length > 0 ? root.textFilter : ""
                color: Theme.accentStrong
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            AppComboBox {
                id: advancedOperator
                Layout.preferredWidth: 104
                textRole: "label"
                valueRole: "mode"
                model: root.operatorModes
                currentIndex: root.operatorIndex
                displayText: currentIndex >= 0 && currentIndex < model.length
                             ? model[currentIndex].label
                             : root.operatorLabel
                onActivated: root.operatorModeRequested(currentValue)
            }

            AppComboBox {
                id: advancedValueSelector
                Layout.fillWidth: true
                enabled: root.operatorIndex >= 0
                model: root.visibleValues
                displayText: root.valuesLoading ? "Carregando" : "Valor"
                onPressedChanged: {
                    if (pressed)
                        root.optionsRequested()
                }
                onActivated: function(index) {
                    if (root.valuesLoading)
                        return
                    if (index < 0 || index >= root.visibleValues.length)
                        return
                    root.selectedValueRequested(advancedValueSelector.textAt(index))
                }
            }

            ActionButton {
                text: root.expandedValues ? "Menos" : "Mais"
                implicitWidth: 52
                enabled: root.hasMoreValues || root.expandedValues
                onClicked: root.expandedValues = !root.expandedValues
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Item { Layout.fillWidth: true }
            ActionButton {
                text: "Selecionar todos"
                implicitWidth: 116
                enabled: root.visibleValues.length > 0
                onClicked: root.loadedValuesFilterRequested("equals")
            }
            ActionButton {
                text: "Excluir todos"
                implicitWidth: 96
                enabled: root.visibleValues.length > 0
                onClicked: root.loadedValuesFilterRequested("different")
            }
            ActionButton {
                text: "Limpar filtro"
                implicitWidth: 94
                onClicked: root.textFilterClearRequested()
            }
        }
    }
}
