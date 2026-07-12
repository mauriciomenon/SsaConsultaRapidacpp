pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    id: root

    required property var smokeController
    required property var rootContentItem
    required property var mainTable
    required property var bottomPane
    required property var statusPill
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
    property int layoutSmokeAttempts: 0

    function itemMetrics(item) {
        const origin = item.mapToItem(root.rootContentItem, 0, 0);
        return {
            x: origin.x,
            y: origin.y,
            width: item.width,
            height: item.height,
            visible: item.visible
        };
    }

    function itemInsideContent(metrics) {
        const tolerance = 0.5;
        return metrics.x >= -tolerance && metrics.y >= -tolerance && metrics.x + metrics.width <= root.rootContentItem.width + tolerance && metrics.y + metrics.height <= root.rootContentItem.height + tolerance;
    }

    function advanceLayoutSmoke() {
        root.layoutSmokeAttempts += 1;
        if (root.layoutSmokeAttempts > 500) {
            layoutSmokeRetry.stop();
            root.smokeController.reportLayoutMetrics({
                success: false,
                error: "layout readiness timeout"
            });
            return;
        }
        const table = root.itemMetrics(root.mainTable);
        const bottom = root.itemMetrics(root.bottomPane);
        const status = root.itemMetrics(root.statusPill);
        if (root.rootContentItem.width <= 0 || root.rootContentItem.height <= 0 || table.height <= 0 || bottom.height <= 0 || status.height <= 0)
            return;

        layoutSmokeRetry.stop();
        const tolerance = 0.5;
        const success = root.itemInsideContent(table) && root.itemInsideContent(bottom) && root.itemInsideContent(status) && status.visible && table.height >= 200 && bottom.height >= 200 && table.y + table.height <= bottom.y + tolerance && bottom.y + bottom.height <= status.y + tolerance;
        root.smokeController.reportLayoutMetrics({
            success: success,
            content: {
                width: root.rootContentItem.width,
                height: root.rootContentItem.height
            },
            table: table,
            bottomPane: bottom,
            status: status
        });
    }

    readonly property Timer layoutSmokeRetry: Timer {
        interval: 10
        repeat: true
        onTriggered: root.advanceLayoutSmoke()
    }

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

        function onLayoutProbeRequested() {
            root.layoutSmokeAttempts = 0;
            root.layoutSmokeRetry.start();
        }
    }
}
