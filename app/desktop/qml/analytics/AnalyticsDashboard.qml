pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Item {
    id: root
    objectName: "analyticsDashboard"

    required property var analyticsViewModel

    readonly property var initialPeriod: analyticsViewModel.currentMonthSelection()
    property int reportFirstYear: initialPeriod.firstYear
    property int reportFirstWeek: initialPeriod.firstWeek
    property int reportLastYear: initialPeriod.lastYear
    property int reportLastWeek: initialPeriod.lastWeek
    property int periodYear: initialPeriod.year
    property int periodMonth: initialPeriod.month
    property int periodScope: 0
    property alias warningText: warningField.text
    readonly property var monthNames: [qsTr("Janeiro"), qsTr("Fevereiro"), qsTr("Marco"), qsTr("Abril"), qsTr("Maio"), qsTr("Junho"), qsTr("Julho"), qsTr("Agosto"), qsTr("Setembro"), qsTr("Outubro"), qsTr("Novembro"), qsTr("Dezembro")]
    readonly property var monthModel: root.periodScope === 1 ? [qsTr("Todos")] : root.monthNames
    readonly property int monthComboIndex: root.periodScope === 1 ? 0 : root.periodMonth - 1
    readonly property var lastCompletePeriod: analyticsViewModel.currentMonthSelection()
    readonly property bool canNavigateNextMonth: root.periodScope === 0 && periodYear * 12 + periodMonth < lastCompletePeriod.year * 12 + lastCompletePeriod.month

    readonly property var chartDefinitions: [
        {
            "key": "executedBySector",
            "title": qsTr("SSAs executadas no periodo"),
            "chartType": "stackedBar"
        },
        {
            "key": "executedMonthly",
            "title": qsTr("Historico de executadas"),
            "chartType": "bar"
        },
        {
            "key": "registeredBySector",
            "title": qsTr("SSAs cadastradas no periodo"),
            "chartType": "bar"
        },
        {
            "key": "registeredMonthly",
            "title": qsTr("Historico de cadastradas"),
            "chartType": "bar"
        },
        {
            "key": "partialAttentionBySector",
            "title": qsTr("Pendentes com atencao parcial"),
            "chartType": "stackedBar"
        },
        {
            "key": "spgBySector",
            "title": qsTr("Estoque SPG"),
            "chartType": "stackedBar"
        },
        {
            "key": "apgBySector",
            "title": qsTr("Estoque APG"),
            "chartType": "stackedBar"
        },
        {
            "key": "aplBySector",
            "title": qsTr("Estoque APL"),
            "chartType": "stackedBar"
        },
        {
            "key": "pendingBySector",
            "title": qsTr("Total pendente"),
            "chartType": "bar"
        },
        {
            "key": "pendingMonthly",
            "title": qsTr("Historico pendente"),
            "chartType": "bar"
        },
        {
            "key": "pendingDeadlinePercentage",
            "title": qsTr("Prazo das pendentes em percentual"),
            "chartType": "percentStackedBar"
        },
        {
            "key": "pendingDeadlineQuantity",
            "title": qsTr("Prazo das pendentes em quantidade"),
            "chartType": "stackedBar"
        },
        {
            "key": "issuedByDivision",
            "title": qsTr("SSAs emitidas"),
            "chartType": "bar"
        },
        {
            "key": "issuedMonthly",
            "title": qsTr("Historico de emitidas"),
            "chartType": "bar"
        }
    ]
    readonly property int chartCount: chartDefinitions.length
    readonly property int columnCount: dashboardFlick.width >= 1080 ? 2 : 1
    readonly property int controlColumnCount: dashboardIsoControls.columns
    readonly property real scrollPosition: dashboardFlick.contentY
    property bool waitingForWarningLoad: false
    property bool dashboardRefreshQueued: false

    function warningValue() {
        if (warningField.text.trim().length === 0)
            return undefined;
        const value = Number(warningField.text);
        return Number.isInteger(value) && value >= 0 && value <= 365 ? value : undefined;
    }

    function dashboardSelection() {
        const selection = {
            "reportFirstYear": root.reportFirstYear,
            "reportFirstWeek": root.reportFirstWeek,
            "reportLastYear": root.reportLastYear,
            "reportLastWeek": root.reportLastWeek
        };
        const warning = root.warningValue();
        if (warning !== undefined)
            selection.warningWindowDays = warning;
        return selection;
    }

    function refreshDashboard() {
        return root.analyticsViewModel.requestDashboard(root.dashboardSelection());
    }

    function applyPeriod(period, scope) {
        root.periodScope = scope === undefined ? 0 : scope;
        root.periodYear = period.year;
        root.periodMonth = period.month;
        root.reportFirstYear = period.firstYear;
        root.reportFirstWeek = period.firstWeek;
        root.reportLastYear = period.lastYear;
        root.reportLastWeek = period.lastWeek;
        root.queueDashboardRefresh();
    }

    function applyCalendarMonth(year, month) {
        if (year * 12 + month > root.lastCompletePeriod.year * 12 + root.lastCompletePeriod.month) {
            root.applyPeriod(root.lastCompletePeriod, 0);
            return;
        }
        root.applyPeriod(root.analyticsViewModel.calendarMonthSelection(year, month), 0);
    }

    function previousMonth() {
        const previous = new Date(root.periodYear, root.periodMonth - 2, 1);
        root.applyCalendarMonth(previous.getFullYear(), previous.getMonth() + 1);
    }

    function nextMonth() {
        if (!root.canNavigateNextMonth)
            return;
        const next = new Date(root.periodYear, root.periodMonth, 1);
        root.applyCalendarMonth(next.getFullYear(), next.getMonth() + 1);
    }

    function useCurrentMonth() {
        root.applyPeriod(root.analyticsViewModel.currentMonthSelection(), 0);
    }

    function applyYearToDate() {
        root.applyPeriod(root.analyticsViewModel.yearToDateSelection(), 1);
    }

    function applyIsoMonth() {
        root.applyPeriod(root.analyticsViewModel.currentIsoMonthSelection(), 2);
    }

    function queueDashboardRefresh() {
        if (root.dashboardRefreshQueued)
            return;
        root.dashboardRefreshQueued = true;
        Qt.callLater(function () {
            root.dashboardRefreshQueued = false;
            root.refreshDashboard();
        });
    }

    function saveWarning() {
        const value = root.warningValue();
        if (value === undefined)
            return false;
        return root.analyticsViewModel.saveWarningWindowDays(value);
    }

    function setWarningText(value) {
        warningField.text = String(value);
    }

    function chartModel(key) {
        const dashboard = root.analyticsViewModel.dashboard;
        return dashboard && dashboard[key] ? dashboard[key] : ({});
    }

    function scrollToBottom() {
        dashboardFlick.contentY = Math.max(0, dashboardFlick.contentHeight - dashboardFlick.height);
    }

    Component.onCompleted: {
        const value = root.analyticsViewModel.warningWindowDays;
        if (value === undefined || value === null) {
            root.waitingForWarningLoad = true;
            root.analyticsViewModel.loadWarningWindowDays();
        } else {
            warningField.text = String(value);
            root.queueDashboardRefresh();
        }
    }

    Connections {
        target: root.analyticsViewModel

        function onWarningWindowDaysChanged() {
            const value = root.analyticsViewModel.warningWindowDays;
            warningField.text = value === undefined || value === null ? "" : String(value);
            if (!root.waitingForWarningLoad)
                root.queueDashboardRefresh();
        }

        function onWarningWindowLoadFinished(successful) {
            if (!root.waitingForWarningLoad)
                return;
            root.waitingForWarningLoad = false;
            const value = root.analyticsViewModel.warningWindowDays;
            warningField.text = value === undefined || value === null ? "" : String(value);
            root.queueDashboardRefresh();
        }
    }

    Flickable {
        id: dashboardFlick

        anchors.fill: parent
        contentWidth: width
        contentHeight: dashboardColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: dashboardColumn

            width: dashboardFlick.width
            spacing: Theme.cardGap

            Rectangle {
                Layout.fillWidth: true
                Layout.margins: Theme.gap
                Layout.bottomMargin: 0
                Layout.preferredHeight: dashboardControls.implicitHeight + Theme.cardGap * 2
                color: Theme.panel
                border.color: Theme.border
                radius: Theme.radius

                ColumnLayout {
                    id: dashboardControls

                    anchors.fill: parent
                    anchors.margins: Theme.cardGap
                    spacing: Theme.gap

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.gap

                        Label {
                            text: qsTr("Periodo mensal")
                            color: Theme.text
                        }
                        ActionButton {
                            objectName: "analyticsDashboardPreviousMonth"
                            Layout.preferredWidth: Theme.controlHeight
                            text: "<"
                            onClicked: root.previousMonth()
                        }
                        AppComboBox {
                            Layout.preferredWidth: 150
                            model: root.monthModel
                            currentIndex: root.monthComboIndex
                            enabled: root.periodScope !== 1
                            onActivated: root.applyCalendarMonth(root.periodYear, currentIndex + 1)
                        }
                        AppSpinBox {
                            Layout.preferredWidth: 88
                            from: 2000
                            to: 2200
                            value: root.periodYear
                            editable: true
                            onValueModified: root.applyCalendarMonth(value, root.periodMonth)
                        }
                        ActionButton {
                            objectName: "analyticsDashboardNextMonth"
                            Layout.preferredWidth: Theme.controlHeight
                            text: ">"
                            enabled: root.canNavigateNextMonth
                            onClicked: root.nextMonth()
                        }
                        ActionButton {
                            Layout.preferredWidth: 160
                            text: qsTr("Ultimo mes completo")
                            onClicked: root.useCurrentMonth()
                        }
                        ActionButton {
                            objectName: "analyticsDashboardYearToDate"
                            Layout.preferredWidth: 140
                            text: qsTr("Ano ate agora")
                            onClicked: root.applyYearToDate()
                        }
                        ActionButton {
                            objectName: "analyticsDashboardIsoMonth"
                            Layout.preferredWidth: 160
                            text: qsTr("Mes ISO completo")
                            onClicked: root.applyIsoMonth()
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    GridLayout {
                        id: dashboardIsoControls

                        Layout.fillWidth: true
                        columns: root.width >= 1200 ? 12 : 6
                        columnSpacing: Theme.gap
                        rowSpacing: Theme.gap

                        Label {
                            text: qsTr("Ano inicial")
                            color: Theme.text
                        }
                        AppSpinBox {
                            objectName: "analyticsDashboardFirstYear"
                            Layout.preferredWidth: 88
                            from: 2000
                            to: 2200
                            locale: Qt.locale("C")
                            value: root.reportFirstYear
                            editable: true
                            onValueModified: {
                                root.reportFirstYear = value;
                                root.periodScope = 0;
                                root.queueDashboardRefresh();
                            }
                        }
                        Label {
                            text: qsTr("Semana inicial")
                            color: Theme.text
                        }
                        AppSpinBox {
                            objectName: "analyticsDashboardFirstWeek"
                            from: 1
                            to: 53
                            value: root.reportFirstWeek
                            editable: true
                            onValueModified: {
                                root.reportFirstWeek = value;
                                root.periodScope = 0;
                                root.queueDashboardRefresh();
                            }
                        }
                        Label {
                            text: qsTr("Ano final")
                            color: Theme.text
                        }
                        AppSpinBox {
                            Layout.preferredWidth: 88
                            from: 2000
                            to: 2200
                            locale: Qt.locale("C")
                            value: root.reportLastYear
                            editable: true
                            onValueModified: {
                                root.reportLastYear = value;
                                root.periodScope = 0;
                                root.queueDashboardRefresh();
                            }
                        }
                        Label {
                            text: qsTr("Semana final")
                            color: Theme.text
                        }
                        AppSpinBox {
                            objectName: "analyticsDashboardLastWeek"
                            from: 1
                            to: 53
                            value: root.reportLastWeek
                            editable: true
                            onValueModified: {
                                root.reportLastWeek = value;
                                root.periodScope = 0;
                                root.queueDashboardRefresh();
                            }
                        }
                        Label {
                            text: qsTr("Janela de alerta em dias")
                            color: Theme.text
                        }
                        AppTextField {
                            id: warningField

                            objectName: "analyticsDashboardWarningField"
                            Layout.preferredWidth: 100
                            placeholderText: qsTr("Sem valor")
                            validator: IntValidator {
                                bottom: 0
                                top: 365
                            }
                            inputMethodHints: Qt.ImhDigitsOnly
                            Accessible.name: qsTr("Janela de alerta em dias")
                        }
                        ActionButton {
                            text: qsTr("Salvar alerta")
                            Layout.preferredWidth: 120
                            enabled: root.warningValue() !== undefined && !root.analyticsViewModel.loading
                            onClicked: root.saveWarning()
                        }
                        ActionButton {
                            objectName: "analyticsDashboardRefresh"
                            Layout.preferredWidth: 126
                            text: qsTr("Atualizar painel")
                            enabled: !root.analyticsViewModel.loading
                            onClicked: root.refreshDashboard()
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.gap
                Layout.rightMargin: Theme.gap
                visible: warningField.text.trim().length === 0
                text: qsTr("Configure a janela de alerta para habilitar os dois graficos de prazo.")
                color: Theme.mutedText
                wrapMode: Text.Wrap
            }

            GridLayout {
                id: dashboardGrid

                Layout.fillWidth: true
                Layout.leftMargin: Theme.gap
                Layout.rightMargin: Theme.gap
                Layout.bottomMargin: Theme.gap
                columns: root.columnCount
                columnSpacing: Theme.cardGap
                rowSpacing: Theme.cardGap

                Repeater {
                    model: root.chartDefinitions

                    AnalyticsChartCard {
                        id: chartCard

                        required property int index
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: preferredCardHeight
                        objectName: "analyticsChartCard-" + index
                        title: modelData.title
                        chartType: modelData.chartType
                        chartModel: root.chartModel(modelData.key)
                    }
                }
            }
        }
    }
}
