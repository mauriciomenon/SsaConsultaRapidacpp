import QtQuick
import SsaConsultaRapida

Item {
    id: root

    property var entries: []

    implicitHeight: entries.length > 0 ? Math.max(18, legendFlow.childrenRect.height) : 0
    visible: entries.length > 0
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Legenda do grafico")

    Flow {
        id: legendFlow

        width: parent.width
        spacing: Theme.gap

        Repeater {
            model: root.entries

            Row {
                id: legendEntry

                required property var modelData

                spacing: Theme.spacingSm
                Accessible.role: Accessible.StaticText
                Accessible.name: modelData.name

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 12
                    height: 12
                    radius: 2
                    color: legendEntry.modelData.color
                    border.color: Theme.border
                }

                Text {
                    text: legendEntry.modelData.hasTrend ? legendEntry.modelData.name + qsTr(" (linha pontilhada: tendencia)") : legendEntry.modelData.name
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                }
            }
        }
    }
}
