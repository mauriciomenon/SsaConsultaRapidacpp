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
    font.pixelSize: Theme.fontSizeBody
    font.bold: false
    implicitWidth: 100
    opacity: enabled ? 1.0 : 0.8
    readonly property bool quietAccent: Theme.gruvbox || Theme.themeName === "ssa-dark"
    readonly property color normalBackground: control.quietAccent ? Theme.panelRaised : Theme.accent
    readonly property color hoverBackground: Theme.accentSoft
    readonly property color downBackground: control.quietAccent ? Theme.accent : Theme.accentStrong
    readonly property color dangerBackground: control.down ? Theme.dangerStrong : control.hovered ? Theme.danger : Theme.dangerSoft
    readonly property color dangerBorder: control.down ? Theme.dangerStrong : control.hovered ? Theme.dangerStrong : Theme.border
    readonly property color normalBorder: control.quietAccent ? Theme.border : (control.hovered ? Theme.accentStrong : Theme.accent)
    readonly property color effectiveBackground: !control.enabled ? Theme.rowAlt : control.danger ? control.dangerBackground : control.down ? control.downBackground : control.hovered ? control.hoverBackground : control.normalBackground
    readonly property color effectiveForeground: {
        if (!control.enabled)
            return Theme.mutedText;
        if (!Theme.refinedNativeTheme)
            return control.danger ? Theme.text : control.quietAccent && !control.down ? Theme.accentStrong : Theme.accentText;
        if (!control.danger && control.quietAccent && !control.hovered && !control.down)
            return Theme.accentStrong;
        return Theme.readableText(control.effectiveBackground);
    }

    background: Rectangle {
        color: control.effectiveBackground
        border.color: !control.enabled ? Theme.border : control.danger ? control.dangerBorder : control.normalBorder
        border.width: 1
        radius: Theme.radius
        Behavior on color {
            ColorAnimation {
                duration: 90
            }
        }
    }

    contentItem: Text {
        text: control.text
        color: control.effectiveForeground
        font.family: control.font.family
        font.pixelSize: control.font.pixelSize
        font.bold: control.font.bold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
