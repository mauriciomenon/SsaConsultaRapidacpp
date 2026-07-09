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
    required property bool valuesLoading
    required property string textFilter
    required property int operatorIndex
    required property string operatorLabel
    required property real cardWidth
    required property real cardHeight
    // Popup width derived from the column's data type (data-driven, see
    // Theme.valuePopupCategory). Every category respects the multi-select
    // structural floor (label + Incluir/Excluir columns + paddings) so the
    // checkboxes never overlap.
    readonly property string valuePopupCategory: Theme.valuePopupCategory(root.row.key)
    readonly property int multiSelectPopupWidth: {
        if (valuePopupCategory === "anomalia")
            return Theme.popupAnomaliaWidth;
        if (valuePopupCategory === "name")
            return Theme.popupNameValueWidth;
        // "code": short codes fit in the structural minimum width.
        return Theme.popupMultiSelectMinWidth;
    }
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
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return 0;
        const origin = openMultiSelectButton.mapToItem(overlayRoot, 0, 0);
        const win = Window.window;
        const boundsWidth = win !== null ? win.width : overlayRoot.width;
        return Theme.clampedPopupX(boundsWidth, origin.x + openMultiSelectButton.width, width);
    }

    function popupY(height) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return openMultiSelectButton.height + Theme.shortcutGap;
        const origin = openMultiSelectButton.mapToItem(overlayRoot, 0, 0);
        const win = Window.window;
        const boundsHeight = win !== null ? win.height : overlayRoot.height;
        return Theme.clampedPopupY(boundsHeight, origin.y, openMultiSelectButton.height, height);
    }

    function popupHeight(preferredHeight) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return preferredHeight;
        const win = Window.window;
        const boundsHeight = win !== null ? win.height : overlayRoot.height;
        return Theme.clampedPopupHeight(boundsHeight, preferredHeight);
    }

    // Resolve X/Y/height together so the popup opens directly below the
    // trigger when possible (shrinking height to fit), and only clamps Y up
    // when opening below cannot fit at all.
    function resolvePopupGeometry(preferredHeight) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return ({
                    x: 0,
                    y: openMultiSelectButton.height + Theme.shortcutGap,
                    h: preferredHeight
                });
        const origin = openMultiSelectButton.mapToItem(overlayRoot, 0, 0);
        const win = Window.window;
        const boundsWidth = win !== null ? win.width : overlayRoot.width;
        const boundsHeight = win !== null ? win.height : overlayRoot.height;
        const originRightX = origin.x + openMultiSelectButton.width;
        const originBottomY = origin.y + openMultiSelectButton.height;
        const x = Theme.clampedPopupX(boundsWidth, originRightX, multiSelectPopupWidth);
        const belowHeight = Theme.clampedPopupHeightBelow(boundsHeight, originBottomY + Theme.shortcutGap, preferredHeight);
        if (belowHeight > 0) {
            return ({
                    x: x,
                    y: originBottomY + Theme.shortcutGap,
                    h: belowHeight
                });
        }
        // Cannot fit below even at min height: clamp Y to the window.
        return ({
                x: x,
                y: Theme.clampedPopupY(boundsHeight, origin.y, openMultiSelectButton.height, preferredHeight),
                h: Theme.clampedPopupHeight(boundsHeight, preferredHeight)
            });
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
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: root.textFilter.length > 0
                text: root.textFilter
                color: Theme.accentStrong
                font.pixelSize: Theme.fontSizeMicro
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.filterRowHeight
            spacing: 4

            AppComboBox {
                id: advancedOperator
                Layout.preferredWidth: Theme.operatorWidth
                Layout.preferredHeight: Theme.filterRowHeight
                leftPadding: 0
                rightPadding: 0
                indicator: null
                popup.width: Theme.operatorPopupWidth
                textRole: "label"
                valueRole: "mode"
                model: root.operatorModes
                currentIndex: root.operatorIndex
                displayText: root.currentOperatorText()
                onActivated: root.operatorModeRequested(currentValue)
            }

            AppComboBox {
                id: advancedValueSelector
                Layout.minimumWidth: Theme.valueMinWidth
                Layout.fillWidth: true
                Layout.preferredWidth: Math.max(Theme.valuePreferredWidth, root.cardWidth * Theme.valuePreferredRatio)
                Layout.preferredHeight: Theme.filterRowHeight
                leftPadding: 7
                rightPadding: 16
                popup.width: root.multiSelectPopupWidth
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
                id: openMultiSelectButton
                text: "..."
                implicitWidth: Theme.commandWidth
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.pixelSize: Theme.fontSizeBody
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
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.bold: true
                font.pixelSize: Theme.fontSizeBody
                ToolTip.visible: hovered
                ToolTip.text: "Limpar filtro"
                ToolTip.delay: 0
                onClicked: root.textFilterClearRequested()
            }
        }
    }

    Popup {
        id: multiSelectPopup
        parent: Overlay.overlay
        x: root.popupX(width)
        y: root.popupY(height)
        width: root.multiSelectPopupWidth
        height: root.popupHeight(360)
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 10

        function updatePopupParent() {
            if (Overlay.overlay !== null)
                parent = Overlay.overlay;
        }

        function updatePopupPosition() {
            updatePopupParent();
            const g = root.resolvePopupGeometry(360);
            x = g.x;
            y = g.y;
            height = g.h;
        }

        onAboutToShow: updatePopupPosition()
        onOpened: updatePopupPosition()
        onVisibleChanged: {
            if (visible)
                updatePopupPosition();
        }
        onWidthChanged: {
            if (visible)
                updatePopupPosition();
        }
        onHeightChanged: {
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: root.row.label !== undefined ? root.row.label : ""
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    font.bold: true
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

                ActionButton {
                    text: "Aplicar"
                    implicitWidth: Theme.applyButtonWidth
                    enabled: !root.valuesLoading
                    onClicked: {
                        root.mixedValuesReplacementRequested(root.includeValues, root.excludeValues);
                        multiSelectPopup.close();
                    }
                }
            }

            AppTextField {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.filterRowHeight
                text: root.popupFilterText
                placeholderText: "Buscar valor"
                font.pixelSize: Theme.fontSizeMicro
                onTextEdited: root.popupFilterText = text
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: "Valor"
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontSizeMicro
                }
                Label {
                    Layout.preferredWidth: Theme.choiceColumnWidth
                    text: "Incluir"
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontSizeMicro
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.preferredWidth: Theme.choiceColumnWidth
                    text: "Excluir"
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontSizeMicro
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
                        font.pixelSize: Theme.fontSizeMicro
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
                                font.pixelSize: Theme.fontSizeMicro
                                elide: Text.ElideRight
                            }

                            Item {
                                Layout.preferredWidth: Theme.choiceColumnWidth
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
                                Layout.preferredWidth: Theme.choiceColumnWidth
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
        }
    }
}
