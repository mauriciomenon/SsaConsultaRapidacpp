pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Popup {
    id: root
    required property Item trigger
    required property string columnKey
    required property string columnLabel
    required property var allValues
    required property bool valuesLoading
    required property int maxValueLength
    required property string textFilter
    property string valuesError: ""

    signal optionsRequested
    signal optionsRetryRequested
    signal mixedValuesReplacementRequested(var includeValues, var excludeValues)

    readonly property int resolvedWidth: Theme.valuePopupWidth(root.columnKey, root.maxValueLength, Overlay.overlay !== null ? Overlay.overlay.width : 0)
    property string filterText: ""
    property var includeValues: []
    property var excludeValues: []
    readonly property var filteredValues: root.filterValues()
    property alias optionList: optionList

    parent: Overlay.overlay
    x: 0
    y: 0
    width: root.resolvedWidth
    height: 0
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 10

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
        if (!root.containsValue(values, value))
            values.push(value);
    }

    function removeValue(values, value) {
        var index = values.indexOf(value);
        if (index >= 0)
            values.splice(index, 1);
    }

    function resetSelections() {
        root.includeValues = root.tokenValues("=");
        root.excludeValues = root.tokenValues("!");
    }

    function filterValues() {
        const needle = root.filterText.trim().toLocaleLowerCase();
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

    function resolvePopupGeometry(preferredHeight) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null)
            return ({
                    x: 0,
                    y: root.trigger.height + Theme.shortcutGap,
                    h: preferredHeight
                });
        const origin = root.trigger.mapToItem(overlayRoot, 0, 0);
        const win = root.trigger.Window.window;
        const boundsWidth = win !== null ? win.width : overlayRoot.width;
        const boundsHeight = win !== null ? win.height : overlayRoot.height;
        const originRightX = origin.x + root.trigger.width;
        const originBottomY = origin.y + root.trigger.height;
        const popupX = Theme.clampedPopupX(boundsWidth, originRightX, root.width);
        const belowHeight = Theme.clampedPopupHeightBelow(boundsHeight, originBottomY + Theme.shortcutGap, preferredHeight);
        if (belowHeight > 0) {
            return ({
                    x: popupX,
                    y: originBottomY + Theme.shortcutGap,
                    h: belowHeight
                });
        }
        return ({
                x: popupX,
                y: Theme.clampedPopupY(boundsHeight, origin.y, root.trigger.height, preferredHeight),
                h: Theme.clampedPopupHeight(boundsHeight, preferredHeight)
            });
    }

    function updatePopupPosition() {
        const chromeHeight = 100;
        const rowHeight = 22;
        const requiredHeight = chromeHeight + Math.max(1, root.filteredValues.length) * rowHeight;
        const preferredHeight = Math.min(requiredHeight, Theme.popupMaxHeight);
        const geometry = root.resolvePopupGeometry(preferredHeight);
        root.x = geometry.x;
        root.y = geometry.y;
        root.height = geometry.h;
    }

    function openForCurrentFilter() {
        root.filterText = "";
        root.resetSelections();
        root.open();
    }

    function activeDelegateCount() {
        var count = 0;
        for (var index = 0; index < optionList.count; ++index) {
            if (optionList.itemAtIndex(index) !== null)
                ++count;
        }
        return count;
    }

    onFilteredValuesChanged: {
        if (root.visible)
            root.updatePopupPosition();
    }
    onAboutToShow: {
        if (root.allValues.length === 0 && !root.valuesLoading && root.valuesError.length === 0)
            root.optionsRequested();
        root.updatePopupPosition();
    }
    onOpened: root.updatePopupPosition()
    onWidthChanged: {
        if (root.visible)
            root.updatePopupPosition();
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
                text: root.columnLabel
                color: Theme.text
                font.pixelSize: Theme.fontSizeBody
                font.bold: true
                elide: Text.ElideRight
            }

            ActionButton {
                text: "Limpar"
                Accessible.name: "Limpar selecao de valores"
                implicitWidth: Theme.popupActionButtonWidth
                onClicked: {
                    root.includeValues = [];
                    root.excludeValues = [];
                }
            }

            ActionButton {
                text: "Aplicar"
                implicitWidth: Theme.popupActionButtonWidth
                enabled: !root.valuesLoading && root.valuesError.length === 0
                Accessible.name: "Aplicar selecao de valores"
                onClicked: {
                    root.mixedValuesReplacementRequested(root.includeValues, root.excludeValues);
                    root.close();
                }
            }
        }

        AppTextField {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.filterRowHeight
            text: root.filterText
            placeholderText: "Buscar valor"
            font.pixelSize: Theme.fontSizeMicro
            onTextEdited: root.filterText = text
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

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                anchors.centerIn: parent
                visible: root.valuesLoading || root.valuesError.length > 0 || root.allValues.length === 0
                spacing: 6

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.valuesLoading ? "Carregando" : root.valuesError.length > 0 ? root.valuesError : "Sem valores carregados"
                    textFormat: Text.PlainText
                    color: root.valuesError.length > 0 ? Theme.danger : Theme.mutedText
                    font.pixelSize: Theme.fontSizeMicro
                    horizontalAlignment: Text.AlignHCenter
                }

                ActionButton {
                    objectName: "advancedTextValueRetryButton_" + root.columnKey
                    visible: root.valuesError.length > 0 && !root.valuesLoading
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Tentar novamente"
                    implicitWidth: Theme.popupActionButtonWidth + 24
                    Accessible.name: "Tentar carregar valores novamente"
                    onClicked: root.optionsRetryRequested()
                }
            }

            ListView {
                id: optionList
                objectName: "advancedTextValueOptionList_" + root.columnKey
                anchors.fill: parent
                clip: true
                spacing: 2
                reuseItems: true
                model: root.visible ? root.filteredValues : null
                ScrollBar.vertical: ScrollBar {}

                delegate: RowLayout {
                    id: optionRow
                    required property string modelData
                    width: ListView.view.width
                    height: 22
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        text: optionRow.modelData
                        textFormat: Text.PlainText
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
                            Accessible.name: "Incluir " + optionRow.modelData
                            checked: root.containsValue(root.includeValues, optionRow.modelData)
                            onClicked: {
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
                            Accessible.name: "Excluir " + optionRow.modelData
                            checked: root.containsValue(root.excludeValues, optionRow.modelData)
                            onClicked: {
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
