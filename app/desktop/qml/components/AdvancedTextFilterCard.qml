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
    padding: 5

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.row.label !== undefined ? root.row.label : ""
                color: Theme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(180, Math.max(80, root.cardWidth * 0.28))
                text: root.textFilter.length > 0 ? root.textFilter : "Sem filtro"
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
                Layout.preferredWidth: 64
                leftPadding: 8
                rightPadding: 18
                textRole: "label"
                valueRole: "mode"
                model: root.operatorModes
                currentIndex: root.operatorIndex
                displayText: root.currentOperatorText()
                onActivated: root.operatorModeRequested(currentValue)
            }

            AppComboBox {
                id: advancedValueSelector
                Layout.fillWidth: true
                enabled: root.operatorIndex >= 0 && !root.valuesLoading
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
                text: root.expandedValues ? "Menos" : "Mais"
                implicitWidth: 52
                enabled: root.hasMoreValues || root.expandedValues
                ToolTip.visible: hovered
                ToolTip.text: root.expandedValues ? qsTr("Mostrar menos valores") : qsTr("Mostrar mais valores")
                ToolTip.delay: 0
                onClicked: root.expandedValues = !root.expandedValues
            }

            ActionButton {
                text: "Lista"
                implicitWidth: 56
                enabled: !root.valuesLoading
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Escolher valores")
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
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Limpar filtro")
                ToolTip.delay: 0
                onClicked: root.textFilterClearRequested()
            }
        }
    }

    Popup {
        id: multiSelectPopup
        x: Math.max(0, root.width - width)
        y: 46
        width: Math.min(520, Math.max(380, root.width - 12))
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
                    Layout.preferredWidth: Math.max(120, multiSelectPopup.availableWidth * 0.34)
                    text: root.row.label !== undefined ? root.row.label : ""
                    color: Theme.text
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: "Marque incluir ou excluir"
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
                    Layout.preferredWidth: 72
                    text: "Incluir"
                    color: Theme.mutedText
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.preferredWidth: 72
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
                                text: optionRow.modelData
                                color: Theme.text
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Item {
                                Layout.preferredWidth: 72
                                Layout.preferredHeight: 24

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
                                Layout.preferredWidth: 72
                                Layout.preferredHeight: 24

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
