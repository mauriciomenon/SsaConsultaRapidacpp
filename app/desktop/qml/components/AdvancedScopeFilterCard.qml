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
    padding: 2
    color: "transparent"
    border.color: "transparent"

    property var selectedReprogrammingValues: []
    readonly property string reprogrammingColumnKey: "num_reprogramacoes"
    property var reprogrammingValueOptions: []
    property bool reprogrammingValueOptionsLoading: false
    readonly property int scopeControlHeight: 22
    readonly property int scopeButtonWidth: 24
    readonly property int scopeFontSize: 11

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
        spacing: 4

        FilterFieldLabel {
            width: 36
            height: root.scopeControlHeight
            text: "Deriv."
            font.pixelSize: root.scopeFontSize
        }
        AppComboBox {
            width: 78
            height: root.scopeControlHeight
            font.pixelSize: root.scopeFontSize
            model: root.derivation.derivationModeOptions
            currentIndex: Math.max(0, root.derivation.derivationModeOptions.indexOf(root.derivation.derivationMode))
            onActivated: {
                root.derivation.derivationMode = currentText;
                root.applyRequested();
            }
        }
        AppCheckBox {
            width: 78
            height: root.scopeControlHeight
            text: "Reprog."
            font.pixelSize: root.scopeFontSize
            checked: root.derivation.onlyReprogrammed
            onToggled: {
                root.derivation.onlyReprogrammed = checked;
                root.applyRequested();
            }
        }

        FilterFieldLabel {
            width: 24
            height: root.scopeControlHeight
            text: "Qtd."
            font.pixelSize: root.scopeFontSize
        }
        AppComboBox {
            id: reprogrammingModeSelector
            width: 62
            height: root.scopeControlHeight
            font.pixelSize: root.scopeFontSize
            leftPadding: 0
            rightPadding: 0
            indicator: null
            popup.width: 170
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
                text: modelData === "lte" ? "<= Menor ou igual" : modelData === "gte" ? ">= Maior ou igual" : "= Igual"
            }
        }
        AppTextField {
            width: 54
            height: root.scopeControlHeight
            font.pixelSize: root.scopeFontSize
            text: root.derivation.reprogrammingEqualsFilter
            placeholderText: "0, 1..."
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.derivation.reprogrammingEqualsFilter = text
            onAccepted: root.applyRequested()
        }
        ActionButton {
            text: "..."
            implicitWidth: root.scopeButtonWidth
            implicitHeight: root.scopeControlHeight
            padding: 0
            font.pixelSize: root.scopeFontSize
            ToolTip.visible: hovered
            ToolTip.text: "Selecionar valores de reprogramacao"
            ToolTip.delay: 0
            onClicked: {
                root.reloadReprogrammingOptionState();
                root.filterViewModel.refreshColumnValueOptionsFor(root.reprogrammingColumnKey);
                root.selectedReprogrammingValues = root.derivation.reprogrammingValues.slice();
                reprogrammingValuesPopup.open();
            }
        }
        ActionButton {
            text: "X"
            implicitWidth: root.scopeButtonWidth
            implicitHeight: root.scopeControlHeight
            padding: 0
            font.pixelSize: root.scopeFontSize
            ToolTip.visible: hovered
            ToolTip.text: "Limpar valores de reprogramacao"
            ToolTip.delay: 0
            onClicked: {
                root.derivation.reprogrammingValues = [];
                root.applyRequested();
            }
        }
        Label {
            width: Math.max(80, root.width - 690)
            height: root.scopeControlHeight
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
