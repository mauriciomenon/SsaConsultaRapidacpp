import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

ApplicationWindow {
    id: root
    required property var mainViewModel
    property var vm: mainViewModel

    width: 1420
    height: 860
    minimumWidth: 1100
    minimumHeight: 720
    visible: true
    title: "SSA Consulta Rapida"
    color: Theme.window
    font.family: Theme.fontFamily

    Component.onCompleted: root.vm.load()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: Theme.gap

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: Theme.panel
            border.color: Theme.border
            radius: Theme.radius

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.gap
                spacing: Theme.gap

                Label {
                    Layout.fillWidth: true
                    text: "SSA Consulta Rapida"
                    font.pixelSize: 18
                    font.bold: true
                    color: Theme.text
                    elide: Text.ElideRight
                }
                ActionButton { text: "SAM"; onClicked: root.vm.commands.openSamHome() }
                ActionButton { text: "Atualizar"; onClicked: root.vm.load() }
                ActionButton { text: "Cancelar"; onClicked: root.vm.cancelCurrentRequest() }
            }
        }

        SearchAndPager {
            Layout.fillWidth: true
            viewModel: root.vm
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            ColumnLayout {
                SplitView.minimumWidth: 720
                SplitView.preferredWidth: 1040
                spacing: Theme.gap

                FilterPanel {
                    Layout.fillWidth: true
                    viewModel: root.vm
                }

                SsaTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    viewModel: root.vm
                }
            }

            DetailsPanel {
                SplitView.minimumWidth: 280
                SplitView.preferredWidth: 360
                SplitView.maximumWidth: 520
                viewModel: root.vm.details
                onOpenRequested: root.vm.openSelectedSsa()
            }
        }

        StatusPill {
            Layout.fillWidth: true
            status: root.vm.status
        }
    }
}
