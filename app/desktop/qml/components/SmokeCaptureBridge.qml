pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    id: root

    required property var smokeController
    required property var preferencesDialog
    required property var filterPanel
    required property var browseViewModel
    signal detailsWindowRequested
    property int detailsOpenAttempts: 0

    function requestDetailsWindowWhenReady() {
        if (root.browseViewModel.details.selectedSsaNumber.length > 0) {
            detailsOpenRetry.stop();
            root.detailsWindowRequested();
            return;
        }
        if (root.detailsOpenAttempts >= 30) {
            detailsOpenRetry.stop();
            console.warn("Smoke capture: details window was not ready after 30 attempts");
            return;
        }
        root.detailsOpenAttempts += 1;
        detailsOpenRetry.restart();
    }

    readonly property Timer detailsOpenRetry: Timer {
        interval: 100
        repeat: false
        onTriggered: root.requestDetailsWindowWhenReady()
    }

    readonly property Connections controllerConnections: Connections {
        target: root.smokeController

        function onOpenPreferencesRequested() {
            root.preferencesDialog.open();
        }

        function onOpenAdvancedFiltersRequested() {
            root.filterPanel.showAdvancedFilters();
        }

        function onOpenDetailsWindowRequested() {
            root.detailsOpenAttempts = 0;
            root.requestDetailsWindowWhenReady();
        }
    }
}
