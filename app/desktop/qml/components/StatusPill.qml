import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var status

    height: 32
    color: Theme.panel
    border.color: root.status.error.length > 0 ? Theme.danger : Theme.border
    radius: Theme.radius

    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: Theme.gap

        BusyIndicator {
            running: root.status.loading
            visible: root.status.loading
            implicitWidth: 20
            implicitHeight: 20
        }
        Label {
            Layout.fillWidth: true
            text: root.status.error.length > 0 ? root.status.error : root.status.message
            color: root.status.error.length > 0 ? Theme.danger : Theme.text
            elide: Text.ElideRight
        }
    }
}
