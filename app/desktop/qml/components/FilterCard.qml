pragma ComponentBehavior: Bound

import QtQuick
import SsaConsultaRapida

Rectangle {
    id: root
    default property alias content: contentHost.data
    property int padding: 8

    color: Theme.panelRaised
    border.color: Theme.border
    radius: Theme.radius

    Item {
        id: contentHost
        anchors.fill: parent
        anchors.margins: root.padding
    }
}
