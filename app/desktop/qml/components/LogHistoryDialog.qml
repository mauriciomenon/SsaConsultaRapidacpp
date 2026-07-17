pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SsaConsultaRapida

Window {
    id: root
    required property var viewModel

    title: "Historico de logs e erros"
    modality: Qt.ApplicationModal
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
    color: Theme.window
    minimumWidth: Math.min(760, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(480, Screen.desktopAvailableHeight)
    width: Math.min(980, Screen.desktopAvailableWidth)
    height: Math.min(680, Screen.desktopAvailableHeight)

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: Theme.gap

            Label {
                text: "Ultimos " + root.viewModel.logs.count + " eventos (limite: 30)"
                color: Theme.text
                font.bold: true
            }

            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal

                ListView {
                    id: eventList
                    objectName: "logHistoryList"
                    SplitView.preferredWidth: 330
                    clip: true
                    model: root.viewModel.logs
                    currentIndex: count > 0 ? 0 : -1

                    delegate: ItemDelegate {
                        required property int index
                        required property string timestamp
                        required property string severity
                        required property string source
                        required property string message
                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        text: timestamp + "  [" + severity + "] " + source + "\n" + message
                        onClicked: eventList.currentIndex = index
                    }
                }

                TextArea {
                    id: eventDetail
                    objectName: "logHistoryDetail"
                    SplitView.fillWidth: true
                    text: eventList.currentIndex >= 0 ? root.viewModel.logs.entryText(eventList.currentIndex) : ""
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    padding: 10
                    background: Rectangle {
                        color: Theme.panelRaised
                        border.color: Theme.border
                        border.width: 1
                        radius: Theme.radius
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                ActionButton {
                    objectName: "copySelectedLogButton"
                    text: "Copiar evento"
                    enabled: eventDetail.text.length > 0
                    onClicked: root.viewModel.copyTextToClipboard(eventDetail.text)
                }
                ActionButton {
                    objectName: "copyAllLogsButton"
                    text: "Copiar todos"
                    enabled: root.viewModel.logs.count > 0
                    onClicked: root.viewModel.copyTextToClipboard(root.viewModel.logs.allText())
                }
                Item {
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: "Fechar"
                    onClicked: root.close()
                }
            }
        }
    }
}
