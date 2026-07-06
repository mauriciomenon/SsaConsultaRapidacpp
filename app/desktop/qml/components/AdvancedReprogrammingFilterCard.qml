pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

// Mirrors the visual layout of AdvancedTextFilterCard but binds to the
// reprogramming contract (derivation.*) instead of the text-filter one.
// Sized to one AdvancedTextFilterGrid cell width by the parent Flow.
FilterCard {
    id: root
    required property var filterViewModel
    required property var derivation
    required property real cardWidth
    required property real cardHeight
    signal applyRequested

    // Constant height received from parent - never derived from childrenRect
    // (binding loop lesson, see RECOVERY_BACKLOG [QML-LAYOUT-LOOP]).
    width: cardWidth
    height: cardHeight
    padding: 3
    color: "transparent"
    border.color: "transparent"

    readonly property string reprogrammingColumnKey: "num_reprogramacoes"
    readonly property string allWithReprogLabel: "Todas com Reprog."
    property var reprogrammingValueOptions: []
    property bool reprogrammingValueOptionsLoading: false
    property var selectedReprogrammingValues: []

    function reloadReprogrammingOptionState() {
        reprogrammingValueOptions = root.filterViewModel.columnValueOptionsFor(reprogrammingColumnKey);
        reprogrammingValueOptionsLoading = root.filterViewModel.columnValueOptionsLoadingFor(reprogrammingColumnKey);
    }

    function currentValueText() {
        if (root.derivation.onlyReprogrammed)
            return root.allWithReprogLabel;
        if (root.derivation.reprogrammingValues.length > 0)
            return root.derivation.reprogrammingValues.join(", ");
        return "";
    }

    function containsSelected(value) {
        return selectedReprogrammingValues.indexOf(value) >= 0;
    }

    function toggleSelected(value, checked) {
        const values = selectedReprogrammingValues.slice();
        const index = values.indexOf(value);
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 3

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: "Reprogramacoes"
                color: Theme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(96, Math.max(60, root.width * 0.16))
                visible: root.currentValueText().length > 0
                text: root.currentValueText()
                color: Theme.accentStrong
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            spacing: 4

            // Operator combo: =, <=, >= (32px - same as AdvancedTextFilterCard.advancedOperator).
            AppComboBox {
                id: operatorSelector
                Layout.preferredWidth: 32
                Layout.preferredHeight: 28
                leftPadding: 0
                rightPadding: 0
                indicator: null
                popup.width: 96
                model: root.derivation.reprogrammingModeOptions
                currentIndex: Math.max(0, root.derivation.reprogrammingModeOptions.indexOf(root.derivation.reprogrammingMode))
                displayText: currentText === "lte" ? "<=" : currentText === "gte" ? ">=" : "="
                onActivated: {
                    root.derivation.reprogrammingMode = currentText;
                    root.applyRequested();
                }
                delegate: ItemDelegate {
                    required property string modelData
                    width: operatorSelector.popup.width
                    text: modelData === "lte" ? "<=" : modelData === "gte" ? ">=" : "="
                }
                ToolTip.visible: hovered
                ToolTip.text: "Operador: = Igual / <= Menor ou igual / >= Maior ou igual"
                ToolTip.delay: 0
            }

            // Value combo: Todas com Reprog. | distinct values from DB.
            AppComboBox {
                id: valueSelector
                Layout.minimumWidth: 60
                Layout.fillWidth: true
                Layout.preferredWidth: Math.max(80, root.width * 0.32)
                Layout.preferredHeight: 28
                leftPadding: 7
                rightPadding: 16
                popup.width: Math.min(360, Math.max(220, root.width * 0.86))
                model: [root.allWithReprogLabel].concat(root.reprogrammingValueOptions)
                displayText: root.currentValueText().length > 0 ? root.currentValueText() : "Valor"
                enabled: root.reprogrammingValueOptions.length > 0 || root.derivation.onlyReprogrammed
                ToolTip.visible: hovered && root.reprogrammingValueOptionsLoading
                ToolTip.text: "Carregando valores"
                ToolTip.delay: 0
                onPressedChanged: {
                    if (pressed)
                        root.filterViewModel.refreshColumnValueOptionsFor(root.reprogrammingColumnKey);
                }
                onActivated: {
                    if (currentText === root.allWithReprogLabel) {
                        root.derivation.onlyReprogrammed = true;
                        root.derivation.reprogrammingValues = [];
                    } else {
                        root.derivation.onlyReprogrammed = false;
                        root.derivation.reprogrammingValues = [currentText];
                    }
                    root.applyRequested();
                }
            }

            ActionButton {
                text: "..."
                implicitWidth: 28
                implicitHeight: 28
                padding: 0
                font.pixelSize: 12
                ToolTip.visible: hovered
                ToolTip.text: "Selecionar valores de reprogramacao"
                ToolTip.delay: 0
                onClicked: {
                    root.reloadReprogrammingOptionState();
                    root.filterViewModel.refreshColumnValueOptionsFor(root.reprogrammingColumnKey);
                    root.selectedReprogrammingValues = root.derivation.reprogrammingValues.slice();
                    valuesPopup.open();
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
                ToolTip.text: "Limpar filtro de reprogramacao"
                ToolTip.delay: 0
                onClicked: {
                    root.derivation.onlyReprogrammed = false;
                    root.derivation.reprogrammingValues = [];
                    root.applyRequested();
                }
            }
        }
    }

    Popup {
        id: valuesPopup
        x: Math.max(0, root.width - width)
        y: 40
        width: Math.min(360, Math.max(240, root.width + 40))
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

            Label {
                Layout.fillWidth: true
                text: "Reprogramacoes"
                color: Theme.text
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
            }

            AppCheckBox {
                Layout.fillWidth: true
                text: root.allWithReprogLabel
                checked: root.derivation.onlyReprogrammed
                onToggled: root.derivation.onlyReprogrammed = checked
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: valuesPopup.availableWidth - 4
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
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                ActionButton {
                    text: "Aplicar"
                    implicitWidth: 88
                    onClicked: {
                        if (root.derivation.onlyReprogrammed)
                            root.selectedReprogrammingValues = [];
                        root.derivation.reprogrammingValues = root.selectedReprogrammingValues;
                        root.applyRequested();
                        valuesPopup.close();
                    }
                }
            }
        }
    }
}
