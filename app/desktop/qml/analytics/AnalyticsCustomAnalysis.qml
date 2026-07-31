pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Item {
    id: root
    objectName: "analyticsCustomAnalysis"

    required property var analyticsViewModel

    readonly property var initialPeriod: analyticsViewModel.currentMonthSelection()
    property int firstYear: initialPeriod.firstYear
    property int firstWeek: initialPeriod.firstWeek
    property int lastYear: initialPeriod.lastYear
    property int lastWeek: initialPeriod.lastWeek
    property bool active: false
    property var selectedDivisions: []
    property var selectedSectors: []
    property var selectedPeople: []
    property bool initialDimensionsRequested: false

    readonly property int metricIndex: metricCombo.currentIndex
    readonly property int grainIndex: grainCombo.currentIndex
    readonly property int breakdownIndex: breakdownCombo.currentIndex
    readonly property int personRoleIndex: roleCombo.currentIndex
    readonly property bool requiresExplicitPeople: breakdownIndex === 2 || breakdownIndex === 3
    readonly property bool requiresWarning: metricIndex === 8
    readonly property bool hasWarning: warningValue() !== undefined
    readonly property bool canAnalyze: !analyticsViewModel.loading && (!requiresExplicitPeople || selectedPeople.length > 0) && (!requiresWarning || hasWarning)

    function warningValue() {
        if (customWarningField.text.trim().length === 0)
            return undefined;
        const value = Number(customWarningField.text);
        return Number.isInteger(value) && value >= 0 && value <= 365 ? value : undefined;
    }

    function requestSelection() {
        const selection = {
            "metric": root.metricIndex,
            "firstYear": root.firstYear,
            "firstWeek": root.firstWeek,
            "lastYear": root.lastYear,
            "lastWeek": root.lastWeek,
            "grain": root.grainIndex,
            "breakdown": root.breakdownIndex,
            "personRole": root.personRoleIndex,
            "divisions": root.selectedDivisions,
            "sectors": root.selectedSectors,
            "people": root.requiresExplicitPeople ? root.selectedPeople : []
        };
        const warning = root.warningValue();
        if (warning !== undefined)
            selection.warningWindowDays = warning;
        return selection;
    }

    function refreshDimensions() {
        return root.analyticsViewModel.requestDimensionValues(root.requestSelection());
    }

    function runAnalysis() {
        if (!root.canAnalyze)
            return false;
        return root.analyticsViewModel.requestCustomSeries(root.requestSelection());
    }

    function toggleValue(values, value, selected) {
        const next = values.slice();
        const index = next.indexOf(value);
        if (selected && index < 0)
            next.push(value);
        else if (!selected && index >= 0)
            next.splice(index, 1);
        return next;
    }

    function selectDivision(value) {
        root.selectedDivisions = root.toggleValue(root.selectedDivisions, value, true);
        root.selectedSectors = [];
        root.selectedPeople = [];
        return root.refreshDimensions();
    }

    function selectSector(value) {
        root.selectedSectors = root.toggleValue(root.selectedSectors, value, true);
        root.selectedPeople = [];
        return root.refreshDimensions();
    }

    function selectPerson(value) {
        root.selectedPeople = root.toggleValue(root.selectedPeople, value, true);
        return true;
    }

    function setBreakdownIndex(index) {
        breakdownCombo.currentIndex = index;
    }

    function setGrainIndex(index) {
        grainCombo.currentIndex = index;
    }

    function changeMetric(index) {
        metricCombo.currentIndex = index;
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        return root.refreshDimensions();
    }

    function changePersonRole(index) {
        roleCombo.currentIndex = index;
        root.selectedPeople = [];
        return root.refreshDimensions();
    }

    function setMetricIndex(index) {
        return root.changeMetric(index);
    }

    function setPersonRoleIndex(index) {
        return root.changePersonRole(index);
    }

    function useCurrentMonth() {
        const period = root.analyticsViewModel.currentMonthSelection();
        root.firstYear = period.firstYear;
        root.firstWeek = period.firstWeek;
        root.lastYear = period.lastYear;
        root.lastWeek = period.lastWeek;
    }

    function configureOverdueByArea() {
        metricCombo.currentIndex = 8;
        grainCombo.currentIndex = 0;
        breakdownCombo.currentIndex = 1;
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        return root.refreshDimensions();
    }

    function isSelected(values, value) {
        return values.indexOf(value) >= 0;
    }

    function dimensionList(key) {
        const values = root.analyticsViewModel.dimensionValues;
        return values && values[key] ? values[key] : [];
    }

    onActiveChanged: {
        if (root.active && !root.initialDimensionsRequested) {
            root.initialDimensionsRequested = true;
            root.refreshDimensions();
        }
    }

    Connections {
        target: root.analyticsViewModel

        function onWarningWindowDaysChanged() {
            const value = root.analyticsViewModel.warningWindowDays;
            customWarningField.text = value === undefined || value === null ? "" : String(value);
        }
    }

    Component.onCompleted: {
        const value = root.analyticsViewModel.warningWindowDays;
        customWarningField.text = value === undefined || value === null ? "" : String(value);
    }

    Flickable {
        id: customFlick

        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: contentColumn

            width: customFlick.width
            spacing: Theme.cardGap

            Rectangle {
                Layout.fillWidth: true
                Layout.margins: Theme.gap
                Layout.bottomMargin: 0
                Layout.preferredHeight: controlsColumn.implicitHeight + Theme.cardGap * 2
                color: Theme.panel
                border.color: Theme.border
                radius: Theme.radius

                ColumnLayout {
                    id: controlsColumn

                    anchors.fill: parent
                    anchors.margins: Theme.cardGap
                    spacing: Theme.gap

                    GridLayout {
                        Layout.fillWidth: true
                        columns: width >= 1040 ? 8 : 4
                        columnSpacing: Theme.gap
                        rowSpacing: Theme.gap

                        Label {
                            text: qsTr("Ano inicial")
                            color: Theme.text
                        }
                        AppSpinBox {
                            Layout.preferredWidth: 88
                            from: 2000
                            to: 2200
                            locale: Qt.locale("C")
                            value: root.firstYear
                            editable: true
                            onValueModified: root.firstYear = value
                        }
                        Label {
                            text: qsTr("Semana inicial")
                            color: Theme.text
                        }
                        AppSpinBox {
                            objectName: "analyticsCustomFirstWeek"
                            from: 1
                            to: 53
                            value: root.firstWeek
                            editable: true
                            onValueModified: root.firstWeek = value
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
                            value: root.lastYear
                            editable: true
                            onValueModified: root.lastYear = value
                        }
                        Label {
                            text: qsTr("Semana final")
                            color: Theme.text
                        }
                        AppSpinBox {
                            objectName: "analyticsCustomLastWeek"
                            from: 1
                            to: 53
                            value: root.lastWeek
                            editable: true
                            onValueModified: root.lastWeek = value
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
                            text: qsTr("Metrica")
                            color: Theme.text
                        }
                        AppComboBox {
                            id: metricCombo

                            Layout.fillWidth: true
                            model: [qsTr("Cadastradas"), qsTr("Executadas"), qsTr("Atencao parcial"), "SPG", "APG", "APL", qsTr("Pendentes"), qsTr("Emitidas"), qsTr("Prazo das pendentes")]
                            onActivated: root.changeMetric(currentIndex)
                        }
                        Label {
                            text: qsTr("Granularidade")
                            color: Theme.text
                        }
                        AppComboBox {
                            id: grainCombo

                            Layout.fillWidth: true
                            model: [qsTr("Acumulado no periodo"), qsTr("Separado por semana ISO"), qsTr("Separado por mes ISO")]
                            onActivated: root.refreshDimensions()
                        }
                        Label {
                            text: qsTr("Quebra")
                            color: Theme.text
                        }
                        AppComboBox {
                            id: breakdownCombo

                            Layout.fillWidth: true
                            model: [qsTr("Divisao"), qsTr("Divisao e setor"), qsTr("Divisao e pessoa"), qsTr("Divisao, setor e pessoa")]
                            onActivated: root.refreshDimensions()
                        }
                        Label {
                            text: qsTr("Papel da pessoa")
                            color: Theme.text
                        }
                        AppComboBox {
                            id: roleCombo

                            Layout.fillWidth: true
                            model: [qsTr("Solicitante"), qsTr("Planejamento/programacao"), qsTr("Execucao")]
                            currentIndex: 2
                            onActivated: root.changePersonRole(currentIndex)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.gap

                        Label {
                            text: qsTr("Janela de alerta em dias")
                            color: Theme.text
                        }
                        TextField {
                            id: customWarningField

                            Layout.preferredWidth: 100
                            placeholderText: qsTr("Sem valor")
                            validator: IntValidator {
                                bottom: 0
                                top: 365
                            }
                            inputMethodHints: Qt.ImhDigitsOnly
                            color: Theme.text
                            font.family: Theme.fontFamily
                            Accessible.name: qsTr("Janela de alerta em dias da analise customizada")

                            background: Rectangle {
                                color: Theme.panelRaised
                                border.color: customWarningField.activeFocus ? Theme.accent : Theme.border
                                radius: Theme.radius
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        ActionButton {
                            text: qsTr("Atualizar opcoes")
                            enabled: !root.analyticsViewModel.loading
                            onClicked: root.refreshDimensions()
                        }
                        ActionButton {
                            text: qsTr("Atrasadas por area")
                            enabled: !root.analyticsViewModel.loading
                            onClicked: root.configureOverdueByArea()
                        }
                        ActionButton {
                            text: qsTr("Gerar grafico")
                            enabled: root.canAnalyze
                            onClicked: root.runAnalysis()
                        }
                        ActionButton {
                            text: qsTr("Cancelar")
                            enabled: root.analyticsViewModel.loading
                            danger: true
                            onClicked: root.analyticsViewModel.cancel()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.requiresExplicitPeople && root.selectedPeople.length === 0
                        text: qsTr("Selecione explicitamente pelo menos uma pessoa para esta quebra.")
                        color: Theme.dangerStrong
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.requiresWarning && !root.hasWarning
                        text: qsTr("Informe a janela de alerta para analisar prazos.")
                        color: Theme.dangerStrong
                        wrapMode: Text.Wrap
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.gap
                Layout.rightMargin: Theme.gap
                columns: width >= 900 ? 3 : 1
                columnSpacing: Theme.cardGap
                rowSpacing: Theme.cardGap

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 176
                    color: Theme.panel
                    border.color: Theme.border
                    radius: Theme.radius

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.gap

                        Label {
                            text: qsTr("Divisoes")
                            color: Theme.text
                            font.bold: true
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.dimensionList("divisions")
                            ScrollBar.vertical: ScrollBar {}

                            delegate: CheckBox {
                                id: divisionOption

                                required property var modelData

                                width: ListView.view.width
                                text: String(modelData)
                                checked: root.isSelected(root.selectedDivisions, String(modelData))
                                onToggled: {
                                    root.selectedDivisions = root.toggleValue(root.selectedDivisions, String(modelData), checked);
                                    root.selectedSectors = [];
                                    root.selectedPeople = [];
                                    root.refreshDimensions();
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 176
                    color: Theme.panel
                    border.color: Theme.border
                    radius: Theme.radius

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.gap

                        Label {
                            text: qsTr("Setores")
                            color: Theme.text
                            font.bold: true
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.dimensionList("sectors")
                            ScrollBar.vertical: ScrollBar {}

                            delegate: CheckBox {
                                id: sectorOption

                                required property var modelData

                                width: ListView.view.width
                                text: String(modelData)
                                checked: root.isSelected(root.selectedSectors, String(modelData))
                                onToggled: {
                                    root.selectedSectors = root.toggleValue(root.selectedSectors, String(modelData), checked);
                                    root.selectedPeople = [];
                                    root.refreshDimensions();
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 176
                    color: Theme.panel
                    border.color: Theme.border
                    radius: Theme.radius

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.gap

                        Label {
                            text: qsTr("Pessoas")
                            color: Theme.text
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: root.dimensionList("people").length === 0
                            text: qsTr("Nenhuma pessoa disponivel para a selecao atual")
                            color: Theme.mutedText
                            wrapMode: Text.Wrap
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.dimensionList("people")
                            ScrollBar.vertical: ScrollBar {}

                            delegate: CheckBox {
                                id: personOption

                                required property var modelData

                                width: ListView.view.width
                                text: String(modelData)
                                checked: root.isSelected(root.selectedPeople, String(modelData))
                                onToggled: root.selectedPeople = root.toggleValue(root.selectedPeople, String(modelData), checked)
                            }
                        }
                    }
                }
            }

            AnalyticsChartCard {
                id: customChart

                Layout.fillWidth: true
                Layout.leftMargin: Theme.gap
                Layout.rightMargin: Theme.gap
                Layout.bottomMargin: Theme.gap
                Layout.preferredHeight: 440
                objectName: "customAnalysisChart"
                title: qsTr("Resultado da analise customizada")
                chartType: root.metricIndex === 8 ? "stackedBar" : "bar"
                chartModel: root.analyticsViewModel.customSeries
            }
        }
    }
}
