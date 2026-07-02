pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var row
    required property var operatorModes
    required property var allValues
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
    readonly property int choiceColumnWidth: 52
    readonly property int commandWidth: 34

    signal optionsRequested
    signal operatorModeRequested(string mode)
    signal selectedValueRequested(string value)
    signal loadedValuesReplacementRequested(string mode)
    signal mixedValuesReplacementRequested(var includeValues, var excludeValues)
    signal textFilterClearRequested

    function tokenValues(prefix) {
        var result = [];
        var parts = root.textFilter.split(",");
        for (var index = 0; index < parts.length; ++index) {
            var token = parts[index].trim();
            if (token.length < 2 || token.charAt(0) !== prefix)
                continue;
            result.push(token.substring(1).trim());
        }
        return result;
    }

    function containsValue(values, value) {
        return values.indexOf(value) >= 0;
    }

    function addUnique(values, value) {
        if (!containsValue(values, value))
            values.push(value);
    }

    function removeValue(values, value) {
        var index = values.indexOf(value);
        if (index >= 0)
            values.splice(index, 1);
    }

    function resetPopupSelections() {
        includeValues = tokenValues("=");
        excludeValues = tokenValues("!");
    }

    function currentOperatorText() {
        if (root.operatorIndex >= 0 && root.operatorIndex < root.operatorModes.length) {
            const label = root.operatorModes[root.operatorIndex].label;
            if (label !== undefined && String(label).length > 0)
                return String(label);
        }
        return root.operatorLabel.length > 0 ? root.operatorLabel : "=";
    }

    property var includeValues: []
    property var excludeValues: []

    width: cardWidth
    height: cardHeight
    padding: 2
    color: "transparent"
    border.color: "transparent"

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 15
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: root.row.label !== undefined ? root.row.label : ""
                color: Theme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(96, Math.max(60, root.cardWidth * 0.16))
                visible: root.textFilter.length > 0
                text: root.textFilter
                color: Theme.accentStrong
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 3

            AppComboBox {
                id: advancedOperator
                Layout.preferredWidth: 34
                leftPadding: 0
                rightPadding: 0
                indicator: null
                popup.width: 96
                textRole: "label"
                valueRole: "mode"
                model: root.operatorModes
                currentIndex: root.operatorIndex
                displayText: root.currentOperatorText()
                onActivated: root.operatorModeRequested(currentValue)
            }

            AppComboBox {
                id: advancedValueSelector
                Layout.minimumWidth: 96
                Layout.fillWidth: true
                Layout.preferredWidth: Math.max(120, root.cardWidth * 0.38)
                leftPadding: 7
                rightPadding: 18
                popup.width: Math.min(560, Math.max(360, root.cardWidth * 0.86))
                enabled: root.operatorIndex >= 0
                model: root.visibleValues
                displayText: root.valuesLoading ? "Carregando" : "Valor"
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
                text: root.expandedValues ? "-" : "+"
                implicitWidth: 26
                implicitHeight: 26
                padding: 0
                font.bold: true
                font.pixelSize: 14
                enabled: root.hasMoreValues || root.expandedValues
                ToolTip.visible: hovered
                ToolTip.text: root.expandedValues ? "Voltar ao combo curto de Valor" : "Mostrar mais valores no combo de Valor; nao aplica filtro"
                ToolTip.delay: 0
                onClicked: root.expandedValues = !root.expandedValues
            }

            ActionButton {
                text: "Sel"
                implicitWidth: root.commandWidth
                implicitHeight: 26
                padding: 0
                font.pixelSize: 11
                enabled: root.operatorIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: "Selecionar valores para incluir ou excluir"
                ToolTip.delay: 0
                onClicked: {
                    if (root.allValues.length === 0)
                        root.optionsRequested();
                    root.resetPopupSelections();
                    multiSelectPopup.open();
                }
            }
            ActionButton {
                text: "X"
                implicitWidth: 30
                implicitHeight: 26
                padding: 0
                font.bold: true
                font.pixelSize: 11
                ToolTip.visible: hovered
                ToolTip.text: "Limpar filtro"
                ToolTip.delay: 0
                onClicked: root.textFilterClearRequested()
            }
        }
    }

    Popup {
        id: multiSelectPopup
        x: Math.max(0, root.width - width)
        y: 40
        width: Math.min(620, Math.max(420, root.width + 80))
        height: 360
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 10

        background: Rectangle {
            color: Theme.panelRaised
            border.color: Theme.border
            radius: Theme.radius
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.preferredWidth: Math.max(130, multiSelectPopup.availableWidth * 0.30)
                    text: root.row.label !== undefined ? root.row.label : ""
                    color: Theme.text
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: "Incluir (=valor) ou excluir (!valor)"
                    color: Theme.mutedText
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }

                ActionButton {
                    text: "Limpar"
                    implicitWidth: 64
                    onClicked: {
                        root.includeValues = [];
                        root.excludeValues = [];
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: "Valor"
                    color: Theme.mutedText
                    font.pixelSize: 11
                }
                Label {
                    Layout.preferredWidth: root.choiceColumnWidth
                    text: "Incluir"
                    color: Theme.mutedText
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.preferredWidth: root.choiceColumnWidth
                    text: "Excluir"
                    color: Theme.mutedText
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: multiSelectPopup.availableWidth - 4
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        visible: root.valuesLoading || root.allValues.length === 0
                        text: root.valuesLoading ? "Carregando" : "Sem valores carregados"
                        color: Theme.mutedText
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Repeater {
                        model: root.allValues

                        RowLayout {
                            id: optionRow
                            required property string modelData
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                text: optionRow.modelData
                                color: Theme.text
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Item {
                                Layout.preferredWidth: root.choiceColumnWidth
                                Layout.preferredHeight: 22

                                AppCheckBox {
                                    anchors.centerIn: parent
                                    text: ""
                                    checked: root.containsValue(root.includeValues, optionRow.modelData)
                                    onToggled: {
                                        var includes = root.includeValues.slice();
                                        var excludes = root.excludeValues.slice();
                                        if (checked) {
                                            root.addUnique(includes, optionRow.modelData);
                                            root.removeValue(excludes, optionRow.modelData);
                                        } else {
                                            root.removeValue(includes, optionRow.modelData);
                                        }
                                        root.includeValues = includes;
                                        root.excludeValues = excludes;
                                    }
                                }
                            }

                            Item {
                                Layout.preferredWidth: root.choiceColumnWidth
                                Layout.preferredHeight: 22

                                AppCheckBox {
                                    anchors.centerIn: parent
                                    text: ""
                                    checked: root.containsValue(root.excludeValues, optionRow.modelData)
                                    onToggled: {
                                        var includes = root.includeValues.slice();
                                        var excludes = root.excludeValues.slice();
                                        if (checked) {
                                            root.addUnique(excludes, optionRow.modelData);
                                            root.removeValue(includes, optionRow.modelData);
                                        } else {
                                            root.removeValue(excludes, optionRow.modelData);
                                        }
                                        root.includeValues = includes;
                                        root.excludeValues = excludes;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                ActionButton {
                    text: "Aplicar"
                    implicitWidth: 88
                    enabled: !root.valuesLoading
                    onClicked: {
                        root.mixedValuesReplacementRequested(root.includeValues, root.excludeValues);
                        multiSelectPopup.close();
                    }
                }
            }
        }
    }
}
