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

    // Resolve X/Y/height together so the popup opens directly below the
    // trigger when possible (shrinking height to fit), and only clamps Y up
    // when opening below cannot fit at all.
    function resolvePopupGeometry(preferredWidth, preferredHeight) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return ({
                    x: 0,
                    y: openValuesButton.height + Theme.shortcutGap,
                    h: preferredHeight
                });
        const origin = openValuesButton.mapToItem(overlayRoot, 0, 0);
        const win = Window.window;
        const boundsWidth = win !== null ? win.width : overlayRoot.width;
        const boundsHeight = win !== null ? win.height : overlayRoot.height;
        const originRightX = origin.x + openValuesButton.width;
        const originBottomY = origin.y + openValuesButton.height;
        const x = Theme.clampedPopupX(boundsWidth, originRightX, preferredWidth);
        const belowHeight = Theme.clampedPopupHeightBelow(boundsHeight, originBottomY + Theme.shortcutGap, preferredHeight);
        if (belowHeight > 0) {
            return ({
                    x: x,
                    y: originBottomY + Theme.shortcutGap,
                    h: belowHeight
                });
        }
        return ({
                x: x,
                y: Theme.clampedPopupY(boundsHeight, origin.y, openValuesButton.height, preferredHeight),
                h: Theme.clampedPopupHeight(boundsHeight, preferredHeight)
            });
    }

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
    property var selectedReprogrammingValues: []
    property bool selectedOnlyReprogrammed: false
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

    function containsSelected(value) {
        return selectedReprogrammingValues.indexOf(value) >= 0;
    }

    function toggleSelected(value, checked) {
        const values = selectedReprogrammingValues.slice();
        const index = values.indexOf(value);
        if (checked) {
            if (index < 0)
                values.push(value);
            selectedOnlyReprogrammed = false;
        }
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
        parent: Overlay.overlay
        x: 0
        y: 0
        width: root.reprogrammingPopupWidth
        height: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 10

        function updatePopupPosition() {
            const g = root.resolvePopupGeometry(root.reprogrammingPopupWidth, Theme.popupPreferredHeight);
            x = g.x;
            y = g.y;
            height = g.h;
        }

        onAboutToShow: {
            root.reloadReprogrammingOptionState();
            if (root.reprogrammingValueOptions.length === 0 && !root.reprogrammingValueOptionsLoading)
                root.filterViewModel.refreshColumnValueOptionsFor(root.reprogrammingColumnKey);
            root.selectedReprogrammingValues = root.derivation.reprogrammingValues.slice();
            root.selectedOnlyReprogrammed = root.derivation.onlyReprogrammed;
            updatePopupPosition();
        }
        onOpened: updatePopupPosition()
        onWidthChanged: {
            if (visible)
                updatePopupPosition();
        }

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
                font.pixelSize: Theme.fontSizeBody
                font.bold: true
                elide: Text.ElideRight
            }

            AppCheckBox {
                Layout.fillWidth: true
                text: root.allWithReprogLabel
                checked: root.selectedOnlyReprogrammed
                onToggled: {
                    root.selectedOnlyReprogrammed = checked;
                    if (checked)
                        root.selectedReprogrammingValues = [];
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Label {
                    anchors.centerIn: parent
                    visible: root.reprogrammingValueOptions.length === 0
                    text: root.reprogrammingValueOptionsLoading ? "Carregando" : "Sem valores"
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontSizeMicro
                    horizontalAlignment: Text.AlignHCenter
                }

                ListView {
                    anchors.fill: parent
                    clip: true
                    spacing: 2
                    reuseItems: true
                    model: valuesPopup.visible ? root.reprogrammingValueOptions : null
                    ScrollBar.vertical: ScrollBar {}

                    delegate: AppCheckBox {
                        required property string modelData
                        width: ListView.view.width
                        height: 22
                        text: modelData
                        checked: root.containsSelected(modelData)
                        onClicked: root.toggleSelected(modelData, checked)
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
                    implicitWidth: Theme.applyButtonWidth
                    onClicked: {
                        if (root.selectedOnlyReprogrammed)
                            root.selectedReprogrammingValues = [];
                        root.derivation.onlyReprogrammed = root.selectedOnlyReprogrammed;
                        root.derivation.reprogrammingValues = root.selectedReprogrammingValues;
                        root.applyRequested();
                        valuesPopup.close();
                    }
                }
            }
        }
    }
}
