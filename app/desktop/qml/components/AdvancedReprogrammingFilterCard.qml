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
    property int reprogrammingMaxValueLength: 0
    readonly property int reprogrammingPopupWidth: Theme.valuePopupWidth(root.reprogrammingColumnKey, root.reprogrammingMaxValueLength, Overlay.overlay !== null ? Overlay.overlay.width : 0)

    function reloadReprogrammingOptionState() {
        reprogrammingValueOptions = root.filterViewModel.columnValueOptionsFor(reprogrammingColumnKey);
        reprogrammingValueOptionsLoading = root.filterViewModel.columnValueOptionsLoadingFor(reprogrammingColumnKey);
        reprogrammingMaxValueLength = root.filterViewModel.columnValueMaxLengthFor(reprogrammingColumnKey);
    }

    function currentValueText() {
        if (root.derivation.onlyReprogrammed)
            return root.allWithReprogLabel;
        if (root.derivation.reprogrammingValues.length > 0)
            return root.derivation.reprogrammingValues.join(", ");
        return "";
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
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(96, Math.max(60, root.width * 0.16))
                visible: root.currentValueText().length > 0
                text: root.currentValueText()
                textFormat: Text.PlainText
                color: Theme.accentStrong
                font.pixelSize: Theme.fontSizeMicro
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.filterRowHeight
            spacing: 4

            // Operator combo: =, <=, >= (32px - same as AdvancedTextFilterCard.advancedOperator).
            AppComboBox {
                id: operatorSelector
                Layout.preferredWidth: Theme.operatorWidth
                Layout.preferredHeight: Theme.filterRowHeight
                leftPadding: 0
                rightPadding: 0
                indicator: null
                popup.width: 96
                model: root.derivation.reprogrammingModeOptions
                currentIndex: Math.max(0, root.derivation.reprogrammingModeOptions.indexOf(root.derivation.reprogrammingMode))
                displayText: currentText === "lte" ? "\u2264" : currentText === "gte" ? "\u2265" : "="
                onActivated: {
                    root.derivation.reprogrammingMode = currentText;
                    root.applyRequested();
                }
                delegate: ItemDelegate {
                    required property string modelData
                    width: operatorSelector.popup.width
                    text: modelData === "lte" ? "\u2264" : modelData === "gte" ? "\u2265" : "="
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
                Layout.preferredHeight: Theme.filterRowHeight
                leftPadding: 7
                rightPadding: 16
                popup.width: root.reprogrammingPopupWidth
                model: [root.allWithReprogLabel].concat(root.reprogrammingValueOptions)
                displayText: root.currentValueText().length > 0 ? root.currentValueText() : "Valor"
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
                id: openValuesButton
                text: "..."
                implicitWidth: 28
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.pixelSize: Theme.fontSizeBody
                Accessible.name: "Selecionar valores de reprogramacao"
                ToolTip.visible: hovered
                ToolTip.text: "Selecionar valores de reprogramacao"
                ToolTip.delay: 0
                onClicked: {
                    valuesPopup.open();
                }
            }
            ActionButton {
                text: "X"
                implicitWidth: 28
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.bold: true
                font.pixelSize: Theme.fontSizeBody
                Accessible.name: "Limpar filtro de reprogramacao"
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

    ReprogrammingValuePopup {
        id: valuesPopup
        filterViewModel: root.filterViewModel
        derivation: root.derivation
        trigger: openValuesButton
        columnKey: root.reprogrammingColumnKey
        allWithReprogLabel: root.allWithReprogLabel
        optionValues: root.reprogrammingValueOptions
        optionsLoading: root.reprogrammingValueOptionsLoading
        maxValueLength: root.reprogrammingMaxValueLength
        onOptionsRequested: root.reloadReprogrammingOptionState()
        onApplyRequested: root.applyRequested()
    }
}
