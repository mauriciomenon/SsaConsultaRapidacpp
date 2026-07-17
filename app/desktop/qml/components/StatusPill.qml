import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var status
    property var browse: null
    property var weekModel: null
    property var activityController: null

    Layout.preferredHeight: 32
    color: Theme.panel
    border.color: root.status.error.length > 0 ? Theme.danger : Theme.border
    border.width: root.status.error.length > 0 ? 2 : 1
    radius: Theme.radius

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: Theme.gap

        BusyIndicator {
            running: root.status.loading
            visible: root.status.loading
            implicitWidth: 20
            implicitHeight: 20
        }
        TextInput {
            objectName: "statusMessageText"
            Layout.fillWidth: true
            text: root.status.error.length > 0 ? root.status.error : root.status.message
            color: root.status.error.length > 0 ? Theme.danger : Theme.text
            font.pixelSize: Theme.fontSizeBody
            font.bold: false
            readOnly: true
            selectByMouse: true
            clip: true
        }

        Label {
            visible: root.weekModel !== null
            text: root.weekModel !== null ? root.weekModel.dateTimeLabel : ""
            color: Theme.mutedText
            font.pixelSize: Theme.fontSizeMicro
        }

        Button {
            objectName: "statusCancelButton"
            visible: root.activityController !== null && (root.activityController.canCancelActivity || root.activityController.cancelingActivity)
            enabled: root.activityController !== null && root.activityController.canCancelActivity
            text: root.activityController !== null && root.activityController.cancelingActivity ? "Cancelando..." : "Cancelar"
            flat: true
            onClicked: root.activityController.requestCancelAll()
        }
    }
}
