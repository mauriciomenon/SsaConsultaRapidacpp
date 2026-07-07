pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property var advanced
    signal applyRequested

    color: Theme.panel
    border.color: "transparent"
    radius: 0

    ScrollView {
        anchors.fill: parent
        anchors.margins: 6
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 5

            AdvancedTextFilterGrid {
                filterViewModel: root.filterViewModel
                textFilters: root.advanced.text
                advanced: root.advanced
                onApplyRequested: root.applyRequested()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                spacing: 6

                Item {
                    Layout.fillWidth: true
                }

                ActionButton {
                    text: "Limpar"
                    implicitWidth: 64
                    implicitHeight: 26
                    padding: 0
                    font.pixelSize: 11
                    ToolTip.visible: hovered
                    ToolTip.text: "Limpar filtros avancados"
                    ToolTip.delay: 0
                    onClicked: root.advanced.clear()
                }

                ActionButton {
                    text: "Aplicar"
                    implicitWidth: 88
                    implicitHeight: 26
                    padding: 0
                    font.pixelSize: 11
                    onClicked: root.applyRequested()
                }
            }
        }
    }
}
