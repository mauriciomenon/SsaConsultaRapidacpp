pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Popup {
    id: root
    required property var viewModel
    readonly property int minPopupWidth: 320
    readonly property int maxPopupWidth: 760
    readonly property int minPopupHeight: 360
    readonly property int maxPopupHeight: 560
    readonly property int fallbackParentWidth: 792
    readonly property int fallbackParentHeight: 680
    readonly property int horizontalMargin: 32
    readonly property int verticalMargin: 120
    readonly property int edgeMargin: 8
    readonly property int popupRightMargin: 16
    readonly property int popupBottomMargin: 16
    readonly property int toolbarOffsetY: 88
    property int preferredY: toolbarOffsetY

    width: Math.max(minPopupWidth, Math.min(maxPopupWidth, (parent ? parent.width : fallbackParentWidth) - horizontalMargin))
    height: Math.max(minPopupHeight, Math.min(maxPopupHeight, (parent ? parent.height : fallbackParentHeight) - verticalMargin))
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    function updatePosition() {
        if (!parent) {
            return;
        }
        x = Math.max(edgeMargin, parent.width - width - popupRightMargin);
        y = Math.max(edgeMargin, Math.min(preferredY, parent.height - height - popupBottomMargin));
    }

    Component.onCompleted: updatePosition()
    onOpened: updatePosition()
    onWidthChanged: updatePosition()
    onHeightChanged: updatePosition()

    Connections {
        target: root.parent

        function onWidthChanged() {
            root.updatePosition();
        }

        function onHeightChanged() {
            root.updatePosition();
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
