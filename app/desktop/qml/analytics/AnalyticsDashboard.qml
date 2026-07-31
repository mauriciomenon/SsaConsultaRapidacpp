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
    property alias warningText: warningField.text

    readonly property var chartDefinitions: [
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
            "key": "issuedByDivision",
            "title": qsTr("SSAs emitidas"),
            "chartType": "bar"
        },
        {
            "key": "issuedMonthly",
            "title": qsTr("Historico de emitidas"),
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
        }
    ]
    readonly property int chartCount: chartDefinitions.length
    readonly property int columnCount: dashboardFlick.width >= 1080 ? 2 : 1
    readonly property int controlColumnCount: dashboardControls.columns
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

    function useCurrentMonth() {
        const period = root.analyticsViewModel.currentMonthSelection();
        root.reportFirstYear = period.firstYear;
        root.reportFirstWeek = period.firstWeek;
        root.reportLastYear = period.lastYear;
        root.reportLastWeek = period.lastWeek;
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

                GridLayout {
                    id: dashboardControls

                    anchors.fill: parent
                    anchors.margins: Theme.cardGap
                    columns: width >= 1200 ? 12 : 6
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
                        onValueModified: root.reportFirstYear = value
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
                        onValueModified: root.reportFirstWeek = value
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
                        onValueModified: root.reportLastYear = value
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
                        onValueModified: root.reportLastWeek = value
                    }
                    Label {
                        text: qsTr("Periodo")
                        color: Theme.text
                    }
                    ActionButton {
                        text: qsTr("Mes atual")
                        onClicked: root.useCurrentMonth()
                    }
                    Label {
                        text: qsTr("Janela de alerta em dias")
                        color: Theme.text
                    }
                    TextField {
                        id: warningField

                        Layout.preferredWidth: 100
                        placeholderText: qsTr("Sem valor")
                        validator: IntValidator {
                            bottom: 0
                            top: 365
                        }
                        inputMethodHints: Qt.ImhDigitsOnly
                        color: Theme.text
                        font.family: Theme.fontFamily
                        Accessible.name: qsTr("Janela de alerta em dias")

                        background: Rectangle {
                            color: Theme.panelRaised
                            border.color: warningField.activeFocus ? Theme.accent : Theme.border
                            radius: Theme.radius
                        }
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
