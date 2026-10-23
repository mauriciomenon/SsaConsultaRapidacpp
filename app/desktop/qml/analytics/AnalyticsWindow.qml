pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SsaConsultaRapida

ApplicationWindow {
    id: root
    objectName: "analyticsWindow"

    required property var analyticsViewModel
    readonly property int currentTabIndex: analyticsTabs.currentIndex

    title: qsTr("Analises de SSA")
    width: Math.min(1180, Screen.desktopAvailableWidth)
    height: Math.min(760, Screen.desktopAvailableHeight)
    minimumWidth: Math.min(760, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(600, Screen.desktopAvailableHeight)
    visible: false
    color: Theme.window
    font.family: Theme.fontFamily

    palette.toolTipBase: Theme.panelRaised
    palette.toolTipText: Theme.text

    // The window lives for the whole session; a month can turn while it is open, so the last
    // complete month is read again each time it is shown or activated.
    function refreshCurrentPeriods() {
        dashboard.lastCompletePeriod = root.analyticsViewModel.currentMonthSelection();
        custom.lastCompleteIsoPeriod = root.analyticsViewModel.currentIsoMonthSelection();
    }

    function open() {
        root.refreshCurrentPeriods();
        root.show();
        root.raise();
        root.requestActivate();
        if (dashboard.reloadPending)
            dashboard.reload();
        custom.ensureDimensions();
    }

    function selectTab(index) {
        if (index >= 0 && index < analyticsTabs.count)
            analyticsTabs.currentIndex = index;
    }

    onActiveChanged: {
        if (root.active)
            root.refreshCurrentPeriods();
    }

    // Closing cancels what is still loading; the next open asks for it again.
    onClosing: {
        if (root.analyticsViewModel.loading) {
            dashboard.reloadPending = true;
            custom.initialDimensionsRequested = false;
        }
        root.analyticsViewModel.cancel();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            TabBar {
                id: analyticsTabs

                Layout.preferredWidth: 420
                Layout.preferredHeight: Theme.controlHeight

                TabButton {
                    text: qsTr("Painel do relatorio")
                    font.family: Theme.fontFamily
                }
                TabButton {
                    text: qsTr("Analise customizada")
                    font.family: Theme.fontFamily
                }
            }

            Item {
                Layout.fillWidth: true
            }

            BusyIndicator {
                running: root.analyticsViewModel.loading
                visible: running
                implicitWidth: Theme.controlHeight
                implicitHeight: Theme.controlHeight
                Accessible.name: qsTr("Carregando analises")
            }

            ActionButton {
                text: qsTr("Cancelar")
                visible: root.analyticsViewModel.loading
                enabled: root.analyticsViewModel.loading
                danger: true
                onClicked: root.analyticsViewModel.cancel()
            }
        }

        Label {
            objectName: "analyticsErrorMessage"
            Layout.fillWidth: true
            visible: root.analyticsViewModel.errorMessage.length > 0
            text: root.analyticsViewModel.errorMessage
            textFormat: Text.PlainText
            color: Theme.dangerStrong
            wrapMode: Text.Wrap
            Accessible.name: qsTr("Falha na analise: ") + text
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: analyticsTabs.currentIndex

            AnalyticsDashboard {
                id: dashboard

                analyticsViewModel: root.analyticsViewModel
            }

            AnalyticsCustomAnalysis {
                id: custom

                analyticsViewModel: root.analyticsViewModel
                active: analyticsTabs.currentIndex === 1
            }
        }
    }
}
