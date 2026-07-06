pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var derivation
    signal applyRequested

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    implicitHeight: Theme.controlHeight + padding * 2
    padding: 2
    color: "transparent"
    border.color: "transparent"

    readonly property int scopeControlHeight: 22
    readonly property int scopeFontSize: 11

    Flow {
        id: scopeFlow
        anchors.fill: parent
        spacing: 4

        FilterFieldLabel {
            width: 36
            height: root.scopeControlHeight
            text: "Deriv."
            font.pixelSize: root.scopeFontSize
        }
        AppComboBox {
            width: 78
            height: root.scopeControlHeight
            font.pixelSize: root.scopeFontSize
            model: root.derivation.derivationModeOptions
            currentIndex: Math.max(0, root.derivation.derivationModeOptions.indexOf(root.derivation.derivationMode))
            onActivated: {
                root.derivation.derivationMode = currentText;
                root.applyRequested();
            }
        }
    }
}
