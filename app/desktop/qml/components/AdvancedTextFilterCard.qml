pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property string key
    required property string label
    required property var operatorModes
    required property var allValues
    required property var visibleValues
    required property bool valuesLoading
    required property int maxValueLength
    required property string textFilter
    required property int operatorIndex
    required property string operatorLabel
    required property real cardWidth
    required property real cardHeight
    signal optionsRequested
    signal operatorModeRequested(string mode)
    signal selectedValueRequested(string value)
    signal mixedValuesReplacementRequested(var includeValues, var excludeValues)
    signal textFilterClearRequested

    function currentOperatorText() {
        if (root.operatorIndex >= 0 && root.operatorIndex < root.operatorModes.length) {
            const label = root.operatorModes[root.operatorIndex].label;
            if (label !== undefined && String(label).length > 0)
                return String(label);
        }
        return root.operatorLabel.length > 0 ? root.operatorLabel : "=";
    }

    function openSmokeValues(values) {
        root.allValues = values;
        root.maxValueLength = String(values[values.length - 1]).length;
        valuePopup.openForCurrentFilter();
        return valuePopup;
    }

    width: cardWidth
    height: cardHeight
    padding: 3
    color: "transparent"
    border.color: "transparent"

    ColumnLayout {
        anchors.fill: parent
        spacing: 3

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            spacing: 4

            Label {
                Layout.preferredWidth: Math.min(180, Math.max(72, implicitWidth + 8))
                text: root.label
                color: Theme.text
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: root.textFilter.length > 0
                text: root.textFilter
                textFormat: Text.PlainText
                color: Theme.accentStrong
                font.pixelSize: Theme.fontSizeMicro
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.filterRowHeight
            spacing: 4

            AppComboBox {
                id: advancedOperator
                Layout.preferredWidth: Theme.operatorWidth
                Layout.preferredHeight: Theme.filterRowHeight
                leftPadding: 0
                rightPadding: 0
                indicator: null
                popup.width: Theme.operatorPopupWidth
                textRole: "label"
                valueRole: "mode"
                model: root.operatorModes
                currentIndex: root.operatorIndex
                displayText: root.currentOperatorText()
                onActivated: root.operatorModeRequested(currentValue)
            }

            AppComboBox {
                id: advancedValueSelector
                Layout.minimumWidth: Theme.valueMinWidth
                Layout.fillWidth: true
                Layout.preferredWidth: Math.max(Theme.valuePreferredWidth, root.cardWidth * Theme.valuePreferredRatio)
                Layout.preferredHeight: Theme.filterRowHeight
                leftPadding: 7
                rightPadding: 16
                popup.width: valuePopup.width
                enabled: root.operatorIndex >= 0
                model: root.visibleValues
                displayText: "Valor"
                ToolTip.visible: hovered && root.valuesLoading
                ToolTip.text: "Carregando valores"
                ToolTip.delay: 0
                onPressedChanged: {
                    if (pressed)
                        root.optionsRequested();
                }
                onActivated: function (index) {
                    if (root.valuesLoading)
                        return;
                    if (index < 0 || index >= root.visibleValues.length)
                        return;
                    root.selectedValueRequested(root.visibleValues[index]);
                }
            }

            ActionButton {
                id: openMultiSelectButton
                text: "..."
                implicitWidth: Theme.commandWidth
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.pixelSize: Theme.fontSizeBody
                enabled: root.operatorIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: "Selecionar valores para incluir ou excluir"
                ToolTip.delay: 0
                onClicked: valuePopup.openForCurrentFilter()
            }
            ActionButton {
                text: "X"
                implicitWidth: 28
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.bold: true
                font.pixelSize: Theme.fontSizeBody
                ToolTip.visible: hovered
                ToolTip.text: "Limpar filtro"
                ToolTip.delay: 0
                onClicked: root.textFilterClearRequested()
            }
        }
    }

    AdvancedTextValuePopup {
        id: valuePopup
        objectName: "advancedTextValuePopup_" + root.key
        trigger: openMultiSelectButton
        columnKey: root.key
        columnLabel: root.label
        allValues: root.allValues
        valuesLoading: root.valuesLoading
        maxValueLength: root.maxValueLength
        textFilter: root.textFilter
        onOptionsRequested: root.optionsRequested()
        onMixedValuesReplacementRequested: function (includeValues, excludeValues) {
            root.mixedValuesReplacementRequested(includeValues, excludeValues);
        }
    }
}
