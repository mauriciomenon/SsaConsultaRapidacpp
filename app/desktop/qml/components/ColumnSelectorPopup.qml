pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Popup {
    id: root
    objectName: "columnSelectorPopup"
    required property var viewModel
    property var trigger: null
    readonly property int minPopupWidth: 320
    readonly property int maxPopupWidth: 760
    readonly property int minPopupHeight: 360
    readonly property int maxPopupHeight: 560

    width: minPopupWidth
    height: minPopupHeight
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    function resolvePopupGeometry() {
        const overlayRoot = Overlay.overlay;
        if (overlayRoot === null || root.trigger === null) {
            return null;
        }
        const origin = root.trigger.mapToItem(overlayRoot, 0, 0);
        const boundsWidth = overlayRoot.width;
        const boundsHeight = overlayRoot.height;
        const popupWidth = Math.max(root.minPopupWidth, Math.min(root.maxPopupWidth, boundsWidth - Theme.popupMargin * 2));
        const popupHeight = Theme.clampedPopupHeight(boundsHeight, root.maxPopupHeight, root.minPopupHeight);
        return {
            x: Theme.clampedPopupX(boundsWidth, origin.x + root.trigger.width, popupWidth),
            y: Theme.clampedPopupY(boundsHeight, origin.y, root.trigger.height, popupHeight),
            width: popupWidth,
            height: popupHeight
        };
    }

    function updateGeometry() {
        const geometry = root.resolvePopupGeometry();
        if (geometry === null) {
            return;
        }
        root.x = geometry.x;
        root.y = geometry.y;
        root.width = geometry.width;
        root.height = geometry.height;
    }

    function openForTrigger(triggerItem) {
        const overlayRoot = Overlay.overlay;
        if (triggerItem === null || overlayRoot === null) {
            return;
        }
        root.trigger = triggerItem;
        root.parent = overlayRoot;
        root.updateGeometry();
        root.open();
    }

    onAboutToShow: updateGeometry()
    onClosed: trigger = null

    Connections {
        target: root.parent

        function onWidthChanged() {
            if (root.visible) {
                root.updateGeometry();
            }
        }

        function onHeightChanged() {
            if (root.visible) {
                root.updateGeometry();
            }
        }
    }

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: "Colunas"
                color: Theme.text
                font.bold: true
                font.pixelSize: Theme.fontSizeTitle
                elide: Text.ElideRight
            }
            ActionButton {
                text: "Fechar"
                onClicked: root.close()
            }
        }

        ColumnConfigList {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.viewModel.columns
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            ActionButton {
                text: "Selecionar tudo"
                onClicked: root.viewModel.columns.selectAll()
            }
            ActionButton {
                text: "Restaurar padrao"
                onClicked: root.viewModel.columnFlow.resetColumnSettings()
            }

            Item {
                Layout.fillWidth: true
            }

            ActionButton {
                text: "Aplicar"
                onClicked: root.viewModel.columnFlow.applyColumnSettings()
            }
            ActionButton {
                text: "Reverter"
                onClicked: root.viewModel.columnFlow.discardColumnSettings()
            }
        }
    }
}
