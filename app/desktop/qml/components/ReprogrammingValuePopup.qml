pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Popup {
    id: root
    objectName: "reprogrammingValuePopup"
    required property var filterViewModel
    required property var derivation
    required property var trigger
    required property string columnKey
    required property string allWithReprogLabel
    property var optionValues: []
    property bool optionsLoading: false
    property int maxValueLength: 0
    property var selectedValues: []
    property bool selectedOnlyReprogrammed: false
    signal optionsRequested
    signal applyRequested

    parent: Overlay.overlay
    x: 0
    y: 0
    width: Theme.valuePopupWidth(root.columnKey, root.maxValueLength, Overlay.overlay !== null ? Overlay.overlay.width : 0)
    height: 0
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 10

    function containsSelected(value) {
        return root.selectedValues.indexOf(value) >= 0;
    }

    function toggleSelected(value, checked) {
        const values = root.selectedValues.slice();
        const index = values.indexOf(value);
        if (checked) {
            if (index < 0)
                values.push(value);
            root.selectedOnlyReprogrammed = false;
        }
        if (!checked && index >= 0)
            values.splice(index, 1);
        root.selectedValues = values;
    }

    function resolvePopupGeometry(preferredWidth, preferredHeight) {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null) {
            return {
                x: 0,
                y: root.trigger.height + Theme.shortcutGap,
                h: preferredHeight
            };
        }
        const origin = root.trigger.mapToItem(overlayRoot, 0, 0);
        const boundsWidth = overlayRoot.width;
        const boundsHeight = overlayRoot.height;
        const originRightX = origin.x + root.trigger.width;
        const originBottomY = origin.y + root.trigger.height;
        const x = Theme.clampedPopupX(boundsWidth, originRightX, preferredWidth);
        const belowHeight = Theme.clampedPopupHeightBelow(boundsHeight, originBottomY + Theme.shortcutGap, preferredHeight);
        if (belowHeight > 0) {
            return {
                x: x,
                y: originBottomY + Theme.shortcutGap,
                h: belowHeight
            };
        }
        return {
            x: x,
            y: Theme.clampedPopupY(boundsHeight, origin.y, root.trigger.height, preferredHeight),
            h: Theme.clampedPopupHeight(boundsHeight, preferredHeight)
        };
    }

    function updatePopupPosition() {
        const geometry = root.resolvePopupGeometry(root.width, Theme.popupPreferredHeight);
        root.x = geometry.x;
        root.y = geometry.y;
        root.height = geometry.h;
    }

    onAboutToShow: {
        root.optionsRequested();
        if (root.optionValues.length === 0 && !root.optionsLoading)
            root.filterViewModel.refreshColumnValueOptionsFor(root.columnKey);
        root.selectedValues = root.derivation.reprogrammingValues.slice();
        root.selectedOnlyReprogrammed = root.derivation.onlyReprogrammed;
        root.updatePopupPosition();
    }
    onOpened: root.updatePopupPosition()
    onWidthChanged: {
        if (visible)
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

        Label {
            Layout.fillWidth: true
            text: "Reprogramacoes"
            textFormat: Text.PlainText
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
                    root.selectedValues = [];
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
                visible: root.optionValues.length === 0
                text: root.optionsLoading ? "Carregando" : "Sem valores"
                textFormat: Text.PlainText
                color: Theme.mutedText
                font.pixelSize: Theme.fontSizeMicro
                horizontalAlignment: Text.AlignHCenter
            }

            ListView {
                id: optionList
                objectName: "reprogrammingValueOptionList"
                anchors.fill: parent
                clip: true
                spacing: 2
                reuseItems: true
                model: root.visible ? root.optionValues : null
                ScrollBar.vertical: ScrollBar {}

                delegate: AppCheckBox {
                    required property int index
                    required property string modelData
                    objectName: "reprogrammingValueOption-" + index
                    width: ListView.view.width
                    height: 22
                    text: modelData
                    checked: root.containsSelected(modelData)
                    onToggled: {
                        root.toggleSelected(modelData, checked);
                        checked = Qt.binding(() => root.containsSelected(modelData));
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
                implicitWidth: Theme.applyButtonWidth
                onClicked: {
                    if (root.selectedOnlyReprogrammed) {
                        root.selectedValues = [];
                        root.derivation.reprogrammingValues = [];
                        root.derivation.onlyReprogrammed = true;
                    } else {
                        root.derivation.onlyReprogrammed = false;
                        root.derivation.reprogrammingValues = root.selectedValues;
                    }
                    root.applyRequested();
                    root.close();
                }
            }
        }
    }
}
