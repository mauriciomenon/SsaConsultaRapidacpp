pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

// Derivation filter: all SSAs, only root SSAs, only derived SSAs, SSAs with derived
// SSAs, or SSAs whose derived SSAs are all closed.
// Binds to derivation.derivationMode ("all" / "root" / "derived" / "has" / "closed").
FilterCard {
    id: root
    required property var derivation
    required property real cardWidth
    required property real cardHeight
    signal applyRequested

    width: cardWidth
    height: cardHeight
    padding: 3
    color: "transparent"
    border.color: "transparent"

    readonly property var modeLabels: ({
            "all": "Todas",
            "root": "So raiz",
            "derived": "So derivadas",
            "has": "Possui derivadas",
            "closed": "Derivadas encerradas"
        })

    ColumnLayout {
        anchors.fill: parent
        spacing: 3

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: "Derivacao"
                color: Theme.text
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(96, Math.max(60, root.cardWidth * 0.16))
                visible: root.derivation.derivationMode !== "all"
                text: root.modeLabels[root.derivation.derivationMode] || ""
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

            AppComboBox {
                id: modeSelector
                objectName: "derivationModeSelector"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.filterRowHeight
                leftPadding: 7
                rightPadding: 16
                model: root.derivation.derivationModeOptions
                currentIndex: Math.max(0, root.derivation.derivationModeOptions.indexOf(root.derivation.derivationMode))
                displayText: root.modeLabels[currentText] || currentText
                onActivated: {
                    root.derivation.derivationMode = currentText;
                    root.applyRequested();
                }
                delegate: ItemDelegate {
                    required property string modelData
                    width: modeSelector.popup.width
                    text: root.modeLabels[modelData] || modelData
                }
                Accessible.name: "Filtro de derivacao"
            }

            ActionButton {
                text: "x"
                implicitWidth: Theme.filterCommandWidth
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.bold: false
                font.pixelSize: Theme.fontSizeMicro
                Accessible.name: "Limpar filtro de derivacao"
                ToolTip.visible: hovered
                ToolTip.text: "Limpar Derivacao"
                ToolTip.delay: 0
                onClicked: {
                    root.derivation.derivationMode = "all";
                    root.applyRequested();
                }
            }
        }
    }
}
