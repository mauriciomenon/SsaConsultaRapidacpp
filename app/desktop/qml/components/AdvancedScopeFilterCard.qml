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
    Layout.preferredHeight: implicitHeight
    implicitHeight: Math.max(Theme.controlHeight + padding * 2, scopeFlow.childrenRect.height + padding * 2)
    padding: 6

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

    Flow {
        id: scopeFlow
        anchors.fill: parent
        spacing: 6

        FilterFieldLabel {
            width: 38
            height: Theme.controlHeight
            text: "Setor"
        }
        AppComboBox {
            id: sectorSelector
            width: 112
            height: Theme.controlHeight
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

        FilterFieldLabel {
            width: 42
            height: Theme.controlHeight
            text: "Deriv."
        }
        AppComboBox {
            width: 92
            height: Theme.controlHeight
            model: root.derivation.derivationModeOptions
            currentIndex: Math.max(0, root.derivation.derivationModeOptions.indexOf(root.derivation.derivationMode))
            onActivated: {
                root.derivation.derivationMode = currentText;
                root.applyRequested();
            }
        }
        AppCheckBox {
            width: 84
            height: Theme.controlHeight
            text: "Reprog."
            checked: root.derivation.onlyReprogrammed
            onToggled: {
                root.derivation.onlyReprogrammed = checked;
                root.applyRequested();
            }
        }

        FilterFieldLabel {
            width: 46
            height: Theme.controlHeight
            text: "Reprog."
        }
        AppComboBox {
            id: reprogrammingModeSelector
            width: 106
            height: Theme.controlHeight
            leftPadding: 8
            rightPadding: 22
            popup.width: 170
            model: root.derivation.reprogrammingModeOptions
            currentIndex: Math.max(0, root.derivation.reprogrammingModeOptions.indexOf(root.derivation.reprogrammingMode))
            displayText: currentText === "lte" ? "<= Menor" : currentText === "gte" ? ">= Maior" : "= Igual"
            onActivated: {
                root.derivation.reprogrammingMode = currentText;
                root.applyRequested();
            }
            delegate: ItemDelegate {
                required property string modelData
                width: reprogrammingModeSelector.width
                text: modelData === "lte" ? "<= Menor ou igual" : modelData === "gte" ? ">= Maior ou igual" : "= Igual"
            }
        }
        AppTextField {
            width: 76
            height: Theme.controlHeight
            text: root.derivation.reprogrammingEqualsFilter
            placeholderText: "0, 1..."
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.derivation.reprogrammingEqualsFilter = text
            onAccepted: root.applyRequested()
        }
        ActionButton {
            text: "Enter"
            implicitWidth: 58
            implicitHeight: Theme.controlHeight
            ToolTip.visible: hovered
            ToolTip.text: "Escolher valores de reprogramacao"
            ToolTip.delay: 0
            onClicked: {
                root.reloadReprogrammingOptionState();
                root.filterViewModel.refreshColumnValueOptionsFor(root.reprogrammingColumnKey);
                root.selectedReprogrammingValues = root.derivation.reprogrammingValues.slice();
                reprogrammingValuesPopup.open();
            }
        }
        ActionButton {
            text: "Del"
            implicitWidth: 40
            implicitHeight: Theme.controlHeight
            ToolTip.visible: hovered
            ToolTip.text: "Limpar valores de reprogramacao"
            ToolTip.delay: 0
            onClicked: {
                root.derivation.reprogrammingValues = [];
                root.applyRequested();
            }
        }
        Label {
            width: Math.max(0, root.width - 734)
            height: Theme.controlHeight
            visible: root.derivation.reprogrammingValues.length > 0
            text: "Valores: " + root.derivation.reprogrammingValues.join(", ")
            color: Theme.accentStrong
            font.pixelSize: 11
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
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
