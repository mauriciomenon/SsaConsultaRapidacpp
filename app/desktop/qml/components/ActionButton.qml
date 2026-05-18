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
    font.bold: false
    implicitWidth: 100
    opacity: enabled ? 1.0 : 0.8
    readonly property color normalBackground: Theme.gruvbox ? Theme.panelRaised : Theme.accent
    readonly property color hoverBackground: Theme.accentSoft
    readonly property color downBackground: Theme.gruvbox ? Theme.accent : Theme.accentStrong
    readonly property color dangerBackground: control.down ? Theme.dangerStrong
                                             : control.hovered ? Theme.danger
                                             : Theme.dangerSoft
    readonly property color dangerBorder: control.down ? Theme.dangerStrong
                                         : control.hovered ? Theme.dangerStrong
                                         : Theme.border
    readonly property color normalBorder: Theme.gruvbox ? Theme.border
                                                        : (control.hovered ? Theme.accentStrong : Theme.accent)

    background: Rectangle {
        color: !control.enabled ? Theme.rowAlt
             : control.danger ? control.dangerBackground
             : control.down ? control.downBackground
             : control.hovered ? control.hoverBackground
             : control.normalBackground
        border.color: !control.enabled
                     ? Theme.border
                     : control.danger ? control.dangerBorder : control.normalBorder
        border.width: 1
        radius: Theme.radius
        Behavior on color { ColorAnimation { duration: 90 } }
    }

    contentItem: Text {
        text: control.text
        color: !control.enabled
               ? Theme.mutedText
               : (control.danger ? Theme.text
                  : Theme.gruvbox && !control.down ? Theme.accentStrong : Theme.accentText)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
