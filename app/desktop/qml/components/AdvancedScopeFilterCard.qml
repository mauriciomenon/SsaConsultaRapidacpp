pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var filterViewModel
    required property var derivation
    signal applyRequested

    Layout.fillWidth: true
    Layout.preferredHeight: 168

    property var selectedReprogrammingValues: []
    readonly property string reprogrammingColumnKey: "num_reprogramacoes"
    property var reprogrammingValueOptions: []
    property bool reprogrammingValueOptionsLoading: false

    function reloadReprogrammingOptionState() {
        reprogrammingValueOptions = root.filterViewModel.columnValueOptionsFor(reprogrammingColumnKey);
        reprogrammingValueOptionsLoading = root.filterViewModel.columnValueOptionsLoadingFor(reprogrammingColumnKey);
    }

    function containsSelected(value) {
        return selectedReprogrammingValues.indexOf(value) >= 0;
    }

    function toggleSelected(value, checked) {
        var values = selectedReprogrammingValues.slice();
        var index = values.indexOf(value);
        if (checked && index < 0)
            values.push(value);
        if (!checked && index >= 0)
            values.splice(index, 1);
        selectedReprogrammingValues = values;
    }

    Component.onCompleted: reloadReprogrammingOptionState()

    Connections {
        target: root.filterViewModel
        function onColumnValueOptionsChangedFor(key) {
            if (key === root.reprogrammingColumnKey)
                root.reloadReprogrammingOptionState();
        }
        function onColumnValueOptionsReset() {
            root.reloadReprogrammingOptionState();
        }
    }

    GridLayout {
        anchors.fill: parent
        columns: 4
        columnSpacing: Theme.gap
        rowSpacing: 6

        FilterFieldLabel {
            text: "Setor Executor"
        }
        AppComboBox {
            id: sectorSelector
            Layout.fillWidth: true
            model: root.filterViewModel.sector.selectorValues
            currentIndex: root.filterViewModel.sector.selectorIndex
            displayText: currentIndex <= 0 ? "Todos" : currentText
            onActivated: {
                root.filterViewModel.sector.quickSector = sectorSelector.currentText;
                root.applyRequested();
            }
            delegate: ItemDelegate {
                required property string modelData
                width: sectorSelector.width
                text: modelData.length === 0 ? "Todos" : modelData
            }
        }
        AppCheckBox {
            Layout.fillWidth: true
            text: "Reprogramadas"
            checked: root.derivation.onlyReprogrammed
            onToggled: {
                root.derivation.onlyReprogrammed = checked;
                root.applyRequested();
            }
        }

        FilterFieldLabel {
            text: "Derivadas"
        }
        AppComboBox {
            Layout.fillWidth: true
            model: root.derivation.derivationModeOptions
            currentIndex: Math.max(0, root.derivation.derivationModeOptions.indexOf(root.derivation.derivationMode))
            onActivated: {
                root.derivation.derivationMode = currentText;
                root.applyRequested();
            }
        }
        FilterFieldLabel {
            text: "Reprogramacoes"
        }
        RowLayout {
            Layout.fillWidth: true

            AppComboBox {
                id: reprogrammingModeSelector
                Layout.preferredWidth: 76
                model: root.derivation.reprogrammingModeOptions
                currentIndex: Math.max(0, root.derivation.reprogrammingModeOptions.indexOf(root.derivation.reprogrammingMode))
                displayText: currentText === "lte" ? "<=" : currentText === "gte" ? ">=" : "="
                onActivated: {
                    root.derivation.reprogrammingMode = currentText;
                    root.applyRequested();
                }
                delegate: ItemDelegate {
                    required property string modelData
                    width: reprogrammingModeSelector.width
                    text: modelData === "lte" ? "<=" : modelData === "gte" ? ">=" : "="
                }
            }
            AppTextField {
                Layout.fillWidth: true
                text: root.derivation.reprogrammingEqualsFilter
                placeholderText: "0, 1, 2..."
                inputMethodHints: Qt.ImhDigitsOnly
                onTextEdited: root.derivation.reprogrammingEqualsFilter = text
                onAccepted: root.applyRequested()
            }
        }

        FilterFieldLabel {
            text: "Valores Reprog."
        }
        ActionButton {
            Layout.fillWidth: true
            text: root.derivation.reprogrammingValues.length > 0 ? root.derivation.reprogrammingValues.join(", ") : "Selecionar"
            onClicked: {
                root.reloadReprogrammingOptionState();
                root.filterViewModel.refreshColumnValueOptionsFor(root.reprogrammingColumnKey);
                root.selectedReprogrammingValues = root.derivation.reprogrammingValues.slice();
                reprogrammingValuesPopup.open();
            }
        }
        ActionButton {
            Layout.fillWidth: true
            text: "Limpar valores"
            onClicked: {
                root.derivation.reprogrammingValues = [];
                root.applyRequested();
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }

    Popup {
        id: reprogrammingValuesPopup
        width: Math.min(360, Math.max(260, root.width * 0.45))
        height: 320
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

            Label {
                Layout.fillWidth: true
                text: "Valores de reprogramacoes"
                color: Theme.text
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: reprogrammingValuesPopup.availableWidth - 4
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        visible: root.reprogrammingValueOptions.length === 0
                        text: root.reprogrammingValueOptionsLoading ? "Carregando" : "Sem valores"
                        color: Theme.mutedText
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Repeater {
                        model: root.reprogrammingValueOptions

                        AppCheckBox {
                            required property string modelData
                            Layout.fillWidth: true
                            text: modelData
                            checked: root.containsSelected(modelData)
                            onToggled: root.toggleSelected(modelData, checked)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item {
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: "Aplicar"
                    implicitWidth: 90
                    onClicked: {
                        root.derivation.reprogrammingValues = root.selectedReprogrammingValues;
                        root.applyRequested();
                        reprogrammingValuesPopup.close();
                    }
                }
            }
        }
    }
}
