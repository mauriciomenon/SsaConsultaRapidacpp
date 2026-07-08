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
    readonly property int compactValueLimit: 18
    readonly property int choiceColumnWidth: 52
    readonly property int commandWidth: 30
    readonly property bool wideValuePopup: row.key === "solicitante" || row.key === "responsavel_programacao" || row.key === "responsavel_execucao" || row.key === "anomalia"
    readonly property int valuePopupWidth: wideValuePopup ? 360 : 160
    readonly property int multiSelectPopupWidth: wideValuePopup ? 520 : 280
    property string popupFilterText: ""

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

    function filteredPopupValues() {
        const needle = root.popupFilterText.trim().toLocaleLowerCase();
        if (needle.length === 0)
            return root.allValues;
        var result = [];
        for (var index = 0; index < root.allValues.length; ++index) {
            const value = String(root.allValues[index]);
            if (value.toLocaleLowerCase().indexOf(needle) >= 0)
                result.push(value);
        }
        return result;
    }

    function popupX(width) {
        const origin = root.mapToItem(Overlay.overlay, 0, 0);
        const leftLimit = 8 - origin.x;
        const rightLimit = Overlay.overlay.width - origin.x - width - 8;
        return Math.max(leftLimit, Math.min(0, rightLimit));
    }

    function popupY(height) {
        const origin = root.mapToItem(Overlay.overlay, 0, 0);
        const preferredY = root.height + 2;
        const bottomLimit = Overlay.overlay.height - origin.y - height - 8;
        const topLimit = 8 - origin.y;
        return Math.max(topLimit, Math.min(preferredY, bottomLimit));
    }

    property var includeValues: []
    property var excludeValues: []

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
                text: root.row.label !== undefined ? root.row.label : ""
                color: Theme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: root.textFilter.length > 0
                text: root.textFilter
                color: Theme.accentStrong
                font.pixelSize: 11
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            spacing: 4

            AppComboBox {
                id: advancedOperator
                Layout.preferredWidth: 32
                Layout.preferredHeight: 28
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
                Layout.minimumWidth: 82
                Layout.fillWidth: true
                Layout.preferredWidth: Math.max(96, root.cardWidth * 0.32)
                Layout.preferredHeight: 28
                leftPadding: 7
                rightPadding: 16
                popup.width: root.valuePopupWidth
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
                text: "..."
                implicitWidth: root.commandWidth
                implicitHeight: 28
                padding: 0
                font.pixelSize: 12
                enabled: root.operatorIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: "Selecionar valores para incluir ou excluir"
                ToolTip.delay: 0
                onClicked: {
                    root.popupFilterText = "";
                    if (root.allValues.length === 0)
                        root.optionsRequested();
                    root.resetPopupSelections();
                    multiSelectPopup.open();
                }
            }
            ActionButton {
                text: "X"
                implicitWidth: 28
                implicitHeight: 28
                padding: 0
                font.bold: true
                font.pixelSize: 12
                ToolTip.visible: hovered
                ToolTip.text: "Limpar filtro"
                ToolTip.delay: 0
                onClicked: root.textFilterClearRequested()
            }
        }
    }

    Popup {
        id: multiSelectPopup
        x: root.popupX(width)
        y: root.popupY(height)
        width: root.multiSelectPopupWidth
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
                    Layout.preferredWidth: root.wideValuePopup ? 150 : 112
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

            AppTextField {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                text: root.popupFilterText
                placeholderText: "Buscar valor"
                font.pixelSize: 11
                onTextEdited: root.popupFilterText = text
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
                        model: root.filteredPopupValues()

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
                                    width: 16
                                    height: 16
                                    spacing: 0
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
                                    width: 16
                                    height: 16
                                    spacing: 0
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

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                color: "transparent"

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: Theme.border
                }

                ActionButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
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
