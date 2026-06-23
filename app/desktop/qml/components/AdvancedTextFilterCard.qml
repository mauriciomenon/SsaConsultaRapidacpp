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

    property var includeValues: []
    property var excludeValues: []

    width: cardWidth
    height: cardHeight
    padding: 6

    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.row.label
                color: Theme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(240, Math.max(96, root.cardWidth * 0.36))
                text: root.textFilter.length > 0 ? "Filtro: " + root.textFilter : "Sem filtro"
                color: root.textFilter.length > 0 ? Theme.accentStrong : Theme.mutedText
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
                displayText: currentIndex >= 0 && currentIndex < model.length ? model[currentIndex].label : root.operatorLabel
                onActivated: root.operatorModeRequested(currentValue)
            }

            AppComboBox {
                id: advancedValueSelector
                Layout.fillWidth: true
                enabled: root.operatorIndex >= 0 && !root.valuesLoading
                model: root.visibleValues
                displayText: root.valuesLoading ? "Carregando" : "Selecionar"
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
                text: root.expandedValues ? "Menos" : "Mais"
                implicitWidth: 52
                enabled: root.hasMoreValues || root.expandedValues
                onClicked: root.expandedValues = !root.expandedValues
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Item {
                Layout.fillWidth: true
            }
            ActionButton {
                text: "Escolher"
                implicitWidth: 88
                enabled: !root.valuesLoading
                onClicked: {
                    if (root.allValues.length === 0)
                        root.optionsRequested();
                    root.resetPopupSelections();
                    multiSelectPopup.open();
                }
            }
            ActionButton {
                text: "Exceto lista"
                implicitWidth: 102
                enabled: root.visibleValues.length > 0
                onClicked: root.loadedValuesReplacementRequested("different")
            }
            ActionButton {
                text: "Limpar"
                implicitWidth: 72
                onClicked: root.textFilterClearRequested()
            }
        }
    }

    Popup {
        id: multiSelectPopup
        x: Math.max(0, root.width - width)
        y: 52
        width: Math.min(520, Math.max(360, root.width - 12))
        height: 420
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
                    Layout.fillWidth: true
                    text: root.row.label
                    color: Theme.text
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                }

                ActionButton {
                    text: "Limpar"
                    implicitWidth: 72
                    onClicked: {
                        root.includeValues = [];
                        root.excludeValues = [];
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 8

                Label {
                    text: "Valor"
                    color: Theme.mutedText
                    font.pixelSize: 11
                }
                Label {
                    text: "Incluir"
                    color: Theme.mutedText
                    font.pixelSize: 11
                }
                Label {
                    text: "Diferente"
                    color: Theme.mutedText
                    font.pixelSize: 11
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
                                text: optionRow.modelData
                                color: Theme.text
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            AppCheckBox {
                                Layout.preferredWidth: 84
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

                            AppCheckBox {
                                Layout.preferredWidth: 84
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
