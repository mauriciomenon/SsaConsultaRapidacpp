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
    property var popupSmokePopup: null
    property var popupSmokeClosedBefore: ({})
    property var popupSmokeOpened: ({})
    property var popupSmokeClosedAfter: ({})
    property int popupSmokeStage: 0
    property int popupSmokeAttempts: 0

    function popupMetrics(popup) {
        return {
            modelCount: popup.optionList.count,
            activeDelegateCount: popup.activeDelegateCount(),
            x: popup.x,
            y: popup.y,
            width: popup.width,
            height: popup.height,
            overlayWidth: popup.parent.width,
            overlayHeight: popup.parent.height,
            visible: popup.visible
        };
    }

    function popupInsideOverlay(metrics) {
        return metrics.x >= 0 && metrics.y >= 0 && metrics.x + metrics.width <= metrics.overlayWidth && metrics.y + metrics.height <= metrics.overlayHeight;
    }

    function requestDetailsWindowWhenReady() {
        if (root.browseViewModel.details.selectedSsaNumber.length > 0) {
            detailsOpenRetry.stop();
            root.detailsWindowRequested();
            root.smokeController.reportDetailsReady();
            return;
        }
        if (root.detailsOpenAttempts >= 30) {
            detailsOpenRetry.stop();
            root.smokeController.reportCaptureFailure();
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

    function failPopupSmoke(errorText) {
        popupSmokeRetry.stop();
        root.popupSmokeStage = 0;
        root.smokeController.reportAdvancedPopupMetrics({
            success: false,
            error: errorText
        });
    }

    function advancePopupSmoke() {
        const popup = root.popupSmokePopup;
        if (popup === null) {
            root.failPopupSmoke("popup disappeared");
            return;
        }
        root.popupSmokeAttempts += 1;
        if (root.popupSmokeAttempts > 500) {
            root.failPopupSmoke("popup readiness timeout");
            return;
        }

        const metrics = root.popupMetrics(popup);
        if (root.popupSmokeStage === 1 && metrics.visible && metrics.modelCount === 5000 && metrics.activeDelegateCount > 0 && metrics.activeDelegateCount < metrics.modelCount) {
            root.popupSmokeOpened = metrics;
            popup.close();
            root.popupSmokeStage = 2;
            return;
        }
        if (root.popupSmokeStage === 2 && !metrics.visible && metrics.modelCount === 0 && metrics.activeDelegateCount === 0) {
            root.popupSmokeClosedAfter = metrics;
            popup.openForCurrentFilter();
            root.popupSmokeStage = 3;
            return;
        }
        if (root.popupSmokeStage !== 3 || !metrics.visible || metrics.activeDelegateCount <= 0)
            return;

        popupSmokeRetry.stop();
        root.popupSmokeStage = 0;
        const grid = root.filterPanel.smokeGridGeometry();
        const success = root.popupSmokeClosedBefore.modelCount === 0 && root.popupSmokeClosedBefore.activeDelegateCount === 0 && root.popupInsideOverlay(root.popupSmokeOpened) && Math.abs(grid.height - grid.contentHeight) < 0.5;
        root.smokeController.reportAdvancedPopupMetrics({
            success: success,
            closedBefore: root.popupSmokeClosedBefore,
            opened: root.popupSmokeOpened,
            closedAfter: root.popupSmokeClosedAfter,
            grid: grid
        });
    }

    readonly property Timer popupSmokeRetry: Timer {
        interval: 10
        repeat: true
        onTriggered: root.advancePopupSmoke()
    }

    readonly property Connections controllerConnections: Connections {
        target: root.smokeController

        function onOpenPreferencesRequested() {
            root.preferencesDialog.open();
        }

        function onOpenAdvancedFiltersRequested() {
            root.filterPanel.showAdvancedFilters();
        }

        function onOpenAdvancedPopupRequested() {
            var values = [];
            for (var index = 0; index < 5000; ++index)
                values.push("value-" + index);
            const popup = root.filterPanel.prepareSmokeValues("situacao", values);
            if (popup === null) {
                root.smokeController.reportAdvancedPopupMetrics({
                    success: false,
                    error: "popup not found"
                });
                return;
            }
            root.popupSmokePopup = popup;
            root.popupSmokeClosedBefore = root.popupMetrics(popup);
            root.popupSmokeOpened = ({});
            root.popupSmokeClosedAfter = ({});
            root.popupSmokeStage = 1;
            root.popupSmokeAttempts = 0;
            popup.openForCurrentFilter();
            root.popupSmokeRetry.start();
        }

        function onOpenDetailsWindowRequested() {
            root.detailsOpenAttempts = 0;
            root.requestDetailsWindowWhenReady();
        }
    }
}
