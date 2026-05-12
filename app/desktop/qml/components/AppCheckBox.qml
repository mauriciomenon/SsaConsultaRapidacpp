import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

CheckBox {
    id: root

    contentItem: Text {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: root.enabled ? Theme.text : Theme.mutedText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
