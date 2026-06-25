pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    property int selectedTabIndex: 0
    readonly property var advanced: filterViewModel.advanced
    readonly property string columnTabText: "Por coluna"
    readonly property string advancedTabText: "Avancados"
    signal applyRequested()

    function showAdvancedFilters() {
        selectedTabIndex = 1
    }

    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            TabBar {
                id: filterTabs
                Layout.preferredWidth: 330
                Layout.preferredHeight: Theme.controlHeight
                currentIndex: root.selectedTabIndex
                onCurrentIndexChanged: root.selectedTabIndex = currentIndex

                FilterTabButton {
                    id: columnTab
                    text: root.columnTabText
                }
                FilterTabButton {
                    id: advancedTab
                    text: root.advancedTabText
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.selectedTabIndex === 0 ? root.columnTabText : root.advancedTabText
                color: Theme.text
                font.pixelSize: 15
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            AppCheckBox {
                text: "Excluir SCA/SES/STE"
                checked: root.filterViewModel.excludeScaSesSte
                onToggled: {
                    root.filterViewModel.excludeScaSesSte = checked;
                    root.applyRequested();
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.selectedTabIndex

            ColumnFilterPanel {
                filterViewModel: root.filterViewModel
                columnViewModel: root.filterViewModel.columns
            }

            AdvancedFilterPanel {
                filterViewModel: root.filterViewModel
                advanced: root.advanced
                onApplyRequested: root.applyRequested()
            }
        }
    }
}
