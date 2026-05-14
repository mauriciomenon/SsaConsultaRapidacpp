import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

Button {
    id: control
    property bool danger: false
    implicitHeight: Theme.controlHeight
    padding: 10
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    font.family: Theme.fontFamily
    font.pixelSize: 12
    font.bold: true
    implicitWidth: 100
    opacity: enabled ? 1.0 : 0.8

    background: Rectangle {
        color: !control.enabled
               ? Theme.rowAlt
               : (control.danger
                  ? (control.down
                     ? Theme.dangerStrong
                     : control.hovered
                       ? Theme.danger
                       : Theme.dangerSoft)
                  : (control.down ? Theme.accentStrong
                                 : control.hovered ? Theme.accentSoft : Theme.accent))
        border.color: !control.enabled
                     ? Theme.border
                     : (control.danger
                        ? Theme.danger
                        : control.hovered ? Theme.accentStrong : Theme.accent)
        border.width: 1
        radius: Theme.radius
        Behavior on color { ColorAnimation { duration: 90 } }
    }

    contentItem: Text {
        text: control.text
        color: !control.enabled
               ? Theme.mutedText
               : (control.danger ? Theme.text : Theme.accentText)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
