pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

// Loading, failure (with retry) or empty text shown in place of a popup value list.
Column {
    id: root

    property bool loading: false
    property string error: ""
    property string emptyText: "Sem valores"
    property string retryObjectName: ""
    signal retryRequested

    spacing: 6

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.loading ? "Carregando" : root.error.length > 0 ? root.error : root.emptyText
        textFormat: Text.PlainText
        color: root.error.length > 0 ? Theme.danger : Theme.mutedText
        font.pixelSize: Theme.fontSizeMicro
        horizontalAlignment: Text.AlignHCenter
    }

    ActionButton {
        objectName: root.retryObjectName
        visible: root.error.length > 0 && !root.loading
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Tentar novamente"
        implicitWidth: Theme.popupActionButtonWidth + 24
        Accessible.name: "Tentar carregar valores novamente"
        onClicked: root.retryRequested()
    }
}
