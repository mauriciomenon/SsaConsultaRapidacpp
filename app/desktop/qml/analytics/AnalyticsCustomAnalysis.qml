pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SsaConsultaRapida

Item {
    id: root
    objectName: "analyticsCustomAnalysis"

    required property var analyticsViewModel

    readonly property var initialPeriod: analyticsViewModel.currentIsoMonthSelection()
    property int firstYear: initialPeriod.firstYear
    property int firstWeek: initialPeriod.firstWeek
    property int lastYear: initialPeriod.lastYear
    property int lastWeek: initialPeriod.lastWeek
    property int periodYear: initialPeriod.year
    property int periodMonth: initialPeriod.month
    property int periodScope: 2
    property bool isoCalendarVisible: false
    property bool active: false
    property bool hideZeroCategories: false
    property bool listZeroValues: false
    property var selectedDivisions: []
    property var selectedSectors: []
    property var selectedPeople: []
    property string divisionSearchText: ""
    property string sectorSearchText: ""
    property bool initialDimensionsRequested: false
    property bool metricInitialized: false
    property bool quickPresetWaitingForDimensions: false

    readonly property int metricIndex: metricCombo.currentIndex
    readonly property int grainIndex: grainCombo.currentIndex
    readonly property int breakdownIndex: breakdownCombo.currentIndex
    readonly property int personRoleIndex: roleCombo.currentIndex
    readonly property int categorySortIndex: sortCombo.currentIndex
    readonly property string chartTitle: root.analyticsViewModel.customChartTitle(root.requestSelection())
    readonly property bool requiresExplicitPeople: breakdownIndex === 2 || breakdownIndex === 3
    readonly property bool usesSectorSelection: breakdownIndex === 1 || breakdownIndex === 3
    readonly property bool usesPeopleSelection: requiresExplicitPeople
    readonly property bool requiresWarning: metricIndex === 8
    readonly property bool hasWarning: warningValue() !== undefined
    readonly property bool canAnalyze: !analyticsViewModel.loading && (!requiresExplicitPeople || selectedPeople.length > 0) && (!requiresWarning || hasWarning)
    readonly property real compactControlScale: root.width < 900 ? 0.92 : 1.0
    readonly property int compactControlSpacing: root.width < 900 ? Math.max(4, Theme.gap - 2) : Theme.gap
    readonly property var monthNames: [qsTr("Janeiro"), qsTr("Fevereiro"), qsTr("Marco"), qsTr("Abril"), qsTr("Maio"), qsTr("Junho"), qsTr("Julho"), qsTr("Agosto"), qsTr("Setembro"), qsTr("Outubro"), qsTr("Novembro"), qsTr("Dezembro")]
    readonly property var monthModel: root.periodScope === 1 ? [qsTr("Todos")] : root.monthNames
    readonly property int monthComboIndex: root.periodScope === 1 ? 0 : root.periodMonth - 1
    readonly property var lastCompleteIsoPeriod: analyticsViewModel.currentIsoMonthSelection()
    readonly property var lastCompletePeriod: root.lastCompleteIsoPeriod
    readonly property bool canNavigateNextMonth: root.periodScope !== 1 && periodYear * 12 + periodMonth < lastCompletePeriod.year * 12 + lastCompletePeriod.month

    component SelectionOption: Rectangle {
        id: selectionOption

        required property string value
        required property string optionObjectName
        property string displayText: value
        property bool checked: false
        signal toggled(bool selected)

        readonly property color optionBackground: checked ? Theme.accentSoft : Theme.panelRaised
        readonly property color optionForeground: Theme.readableText(optionBackground, checked ? Theme.accentStrong : Theme.text)
        readonly property real textContrast: Theme.contrastRatio(optionForeground, optionBackground)
        readonly property bool focusHighlighted: activeFocus

        objectName: optionObjectName
        width: ListView.view ? ListView.view.width : implicitWidth
        implicitHeight: Math.max(32, optionLabel.implicitHeight + 10)
        color: optionBackground
        border.color: focusHighlighted ? Theme.accentStrong : checked ? Theme.accent : Theme.border
        border.width: focusHighlighted ? 2 : 1
        radius: Theme.radius
        activeFocusOnTab: true
        Accessible.role: Accessible.CheckBox
        Accessible.name: displayText
        Accessible.checked: checked

        Rectangle {
            id: optionIndicator

            anchors.left: parent.left
            anchors.leftMargin: 7
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            radius: 4
            color: selectionOption.checked ? Theme.accent : Theme.panel
            border.color: selectionOption.checked ? Theme.accentStrong : Theme.border

            Rectangle {
                visible: selectionOption.checked
                anchors.centerIn: parent
                width: 8
                height: 8
                radius: 2
                color: Theme.readableText(optionIndicator.color, Theme.accentText)
            }
        }

        Text {
            id: optionLabel

            anchors.left: optionIndicator.right
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.rightMargin: 7
            anchors.verticalCenter: parent.verticalCenter
            text: selectionOption.displayText
            textFormat: Text.PlainText
            color: selectionOption.optionForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.Normal
            wrapMode: Text.Wrap
        }

        TapHandler {
            onTapped: selectionOption.toggled(!selectionOption.checked)
        }
        Keys.onSpacePressed: selectionOption.toggled(!selectionOption.checked)
        Keys.onReturnPressed: selectionOption.toggled(!selectionOption.checked)
    }

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
            // Division-only charts must not AND sector/person UI checks into the series.
            "sectors": root.usesSectorSelection ? root.selectedSectors : [],
            "people": root.usesPeopleSelection ? root.selectedPeople : [],
            "categorySort": root.categorySortIndex
        };
        const warning = root.warningValue();
        if (warning !== undefined)
            selection.warningWindowDays = warning;
        return selection;
    }

    function refreshDimensions() {
        return root.analyticsViewModel.requestDimensionValues(root.requestSelection());
    }

    function invalidateCustomChart() {
        root.analyticsViewModel.clearCustomSeries();
    }

    function invalidateCustomAnalysisIfActive() {
        root.invalidateCustomChart();
        if (root.active || root.initialDimensionsRequested)
            return root.refreshDimensions();
        return true;
    }

    function invalidateCustomAnalysis() {
        root.invalidateCustomChart();
        return root.refreshDimensions();
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
        return root.invalidateCustomAnalysis();
    }

    function selectSector(value) {
        root.selectedSectors = root.toggleValue(root.selectedSectors, value, true);
        root.selectedPeople = [];
        return root.invalidateCustomAnalysis();
    }

    function selectPerson(value) {
        root.selectedPeople = root.toggleValue(root.selectedPeople, value, true);
        root.invalidateCustomChart();
        return true;
    }

    function setBreakdownIndex(index) {
        breakdownCombo.currentIndex = index;
        root.invalidateCustomAnalysis();
    }

    function setGrainIndex(index) {
        grainCombo.currentIndex = index;
        root.invalidateCustomAnalysis();
    }

    function changeMetric(index) {
        metricCombo.currentIndex = index;
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        return root.invalidateCustomAnalysis();
    }

    function changePersonRole(index) {
        roleCombo.currentIndex = index;
        root.selectedPeople = [];
        return root.invalidateCustomAnalysis();
    }

    function setMetricIndex(index) {
        return root.changeMetric(index);
    }

    function setPersonRoleIndex(index) {
        return root.changePersonRole(index);
    }

    function applyPeriod(period, scope) {
        root.periodScope = scope === undefined ? 0 : scope;
        root.periodYear = period.year;
        root.periodMonth = period.month;
        root.firstYear = period.firstYear;
        root.firstWeek = period.firstWeek;
        root.lastYear = period.lastYear;
        root.lastWeek = period.lastWeek;
        root.onPeriodEdited();
    }

    function onPeriodEdited() {
        root.invalidateCustomAnalysisIfActive();
    }

    function applyIsoReferenceMonth(year, month) {
        if (year * 12 + month > root.lastCompleteIsoPeriod.year * 12 + root.lastCompleteIsoPeriod.month) {
            root.applyPeriod(root.lastCompleteIsoPeriod, 2);
            return;
        }
        root.applyPeriod(root.analyticsViewModel.isoMonthSelection(year, month), 2);
    }

    function applySelectedMonth(year, month) {
        root.applyIsoReferenceMonth(year, month);
    }

    function previousMonth() {
        const previous = new Date(root.periodYear, root.periodMonth - 2, 1);
        root.applySelectedMonth(previous.getFullYear(), previous.getMonth() + 1);
    }

    function nextMonth() {
        if (!root.canNavigateNextMonth)
            return;
        const next = new Date(root.periodYear, root.periodMonth, 1);
        root.applySelectedMonth(next.getFullYear(), next.getMonth() + 1);
    }

    function applyLastMonth() {
        root.applyPeriod(root.analyticsViewModel.currentIsoMonthSelection(), 2);
    }

    function applyLastTwelveMonths() {
        root.applyPeriod(root.analyticsViewModel.lastTwelveIsoMonthsSelection(), 3);
    }

    function toggleIsoCalendar() {
        root.isoCalendarVisible = !root.isoCalendarVisible;
    }

    function applyYearToDate() {
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        root.quickPresetWaitingForDimensions = root.usesSectorSelection || root.usesPeopleSelection;
        root.applyPeriod(root.analyticsViewModel.yearToDateSelection(), 1);
        if (!root.quickPresetWaitingForDimensions)
            return;
    }

    function notifyFirstWeekEdited(week) {
        root.firstWeek = week;
        root.onPeriodEdited();
    }

    function selectAllDivisions() {
        root.selectedDivisions = root.dimensionList("divisions").slice();
        root.selectedSectors = [];
        root.selectedPeople = [];
        root.invalidateCustomAnalysis();
    }

    function clearDivisions() {
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        root.invalidateCustomAnalysis();
    }

    function selectAllSectors() {
        root.selectedSectors = root.dimensionList("sectors").slice();
        root.selectedPeople = [];
        root.invalidateCustomAnalysis();
    }

    function clearSectors() {
        root.selectedSectors = [];
        root.selectedPeople = [];
        root.invalidateCustomAnalysis();
    }

    function selectAllPeople() {
        root.selectedPeople = root.dimensionList("people").slice();
        root.invalidateCustomChart();
    }

    function clearPeople() {
        root.selectedPeople = [];
        root.invalidateCustomChart();
    }

    function pruneOrphanSelections() {
        const divisions = root.dimensionList("divisions");
        root.selectedDivisions = root.selectedDivisions.filter(value => divisions.indexOf(value) >= 0);
        const sectors = root.dimensionList("sectors");
        root.selectedSectors = root.selectedSectors.filter(value => sectors.indexOf(value) >= 0);
        const people = root.dimensionList("people");
        root.selectedPeople = root.selectedPeople.filter(value => people.indexOf(value) >= 0);
    }

    function configureOverdueByArea() {
        metricCombo.currentIndex = 8;
        grainCombo.currentIndex = 0;
        breakdownCombo.currentIndex = 1;
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        return root.invalidateCustomAnalysis();
    }

    function configureExecutedByPerson() {
        metricCombo.currentIndex = 1;
        grainCombo.currentIndex = 0;
        breakdownCombo.currentIndex = 3;
        roleCombo.currentIndex = 2;
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        return root.invalidateCustomAnalysis();
    }

    function configureExecutedBySectorWeek() {
        metricCombo.currentIndex = 1;
        grainCombo.currentIndex = 0;
        breakdownCombo.currentIndex = 1;
        roleCombo.currentIndex = 2;
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        root.applyPeriod(root.analyticsViewModel.currentIsoWeekSelection());
    }

    function configureExecutedBySectorMonth() {
        metricCombo.currentIndex = 1;
        grainCombo.currentIndex = 0;
        breakdownCombo.currentIndex = 1;
        roleCombo.currentIndex = 2;
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        root.applyPeriod(root.analyticsViewModel.currentIsoMonthSelection(), 2);
    }

    function configureExecutedBySectorPerson() {
        metricCombo.currentIndex = 1;
        grainCombo.currentIndex = 0;
        breakdownCombo.currentIndex = 3;
        roleCombo.currentIndex = 2;
        root.selectedDivisions = [];
        root.selectedSectors = [];
        root.selectedPeople = [];
        root.applyPeriod(root.analyticsViewModel.currentIsoWeekSelection());
    }

    function isoWeekLabel(year, week) {
        return String(year) + String(week).padStart(2, "0");
    }

    function runQuickPreset(configureFn) {
        root.quickPresetWaitingForDimensions = true;
        configureFn();
        if (root.requiresExplicitPeople)
            return true;
        root.quickPresetWaitingForDimensions = false;
        return root.runAnalysis();
    }

    function isSelected(values, value) {
        return values.indexOf(value) >= 0;
    }

    function dimensionList(key) {
        const values = root.analyticsViewModel.dimensionValues;
        return values && values[key] ? values[key] : [];
    }

    function organizationalUnitLabel(value) {
        return root.analyticsViewModel.organizationalUnitLabel(String(value));
    }

    function filteredDimensionList(key, query) {
        const values = root.dimensionList(key);
        const normalized = query.trim().toLocaleLowerCase();
        if (normalized.length === 0)
            return values;
        return values.filter(value => root.organizationalUnitLabel(value).toLocaleLowerCase().includes(normalized));
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

        function onDimensionValuesChanged() {
            root.pruneOrphanSelections();
            if (!root.quickPresetWaitingForDimensions)
                return;
            root.quickPresetWaitingForDimensions = false;
            if (root.usesSectorSelection)
                root.selectedSectors = root.dimensionList("sectors").slice();
            if (root.usesPeopleSelection)
                root.selectedPeople = root.dimensionList("people").slice();
            root.runAnalysis();
        }
    }

    Component.onCompleted: {
        const value = root.analyticsViewModel.warningWindowDays;
        customWarningField.text = value === undefined || value === null ? "" : String(value);
        if (!root.metricInitialized) {
            root.metricInitialized = true;
            metricCombo.currentIndex = 1;
        }
    }

    Flickable {
        id: customFlick

        objectName: "analyticsCustomFlick"
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

                    Flickable {
                        id: customPeriodControls

                        objectName: "analyticsCustomPeriodControls"
                        Layout.fillWidth: true
                        Layout.preferredHeight: customPeriodRow.implicitHeight + (contentWidth > width ? customPeriodScrollbar.implicitHeight : 0)
                        contentWidth: Math.max(width, customPeriodRow.implicitWidth)
                        contentHeight: customPeriodRow.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true
                        ScrollBar.horizontal: ScrollBar {
                            id: customPeriodScrollbar

                            policy: customPeriodControls.contentWidth > customPeriodControls.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                        }

                        RowLayout {
                            id: customPeriodRow

                            width: implicitWidth
                            height: implicitHeight
                            spacing: root.compactControlSpacing

                            Label {
                                text: qsTr("Periodo mensal")
                                color: Theme.text
                            }
                            ActionButton {
                                objectName: "analyticsCustomPreviousMonth"
                                Layout.preferredWidth: Theme.controlHeight * root.compactControlScale
                                text: "<"
                                onClicked: root.previousMonth()
                            }
                            AppComboBox {
                                id: monthCombo

                                Layout.preferredWidth: 150 * root.compactControlScale
                                model: root.monthModel
                                currentIndex: root.monthComboIndex
                                enabled: root.periodScope !== 1
                                onActivated: root.applySelectedMonth(root.periodYear, currentIndex + 1)
                            }
                            AppSpinBox {
                                Layout.preferredWidth: 88 * root.compactControlScale
                                from: 2000
                                to: 2200
                                value: root.periodYear
                                editable: true
                                onValueModified: root.applySelectedMonth(value, root.periodMonth)
                            }
                            ActionButton {
                                objectName: "analyticsCustomNextMonth"
                                Layout.preferredWidth: Theme.controlHeight * root.compactControlScale
                                text: ">"
                                enabled: root.canNavigateNextMonth
                                onClicked: root.nextMonth()
                            }
                            ActionButton {
                                objectName: "analyticsCustomLastMonth"
                                Layout.preferredWidth: 160 * root.compactControlScale
                                text: qsTr("Ultimo mes")
                                onClicked: root.applyLastMonth()
                            }
                            ActionButton {
                                objectName: "analyticsCustomYearToDate"
                                Layout.preferredWidth: 140 * root.compactControlScale
                                text: qsTr("Ano ate agora")
                                onClicked: root.applyYearToDate()
                            }
                            ActionButton {
                                objectName: "analyticsCustomLastTwelveMonths"
                                Layout.preferredWidth: 160 * root.compactControlScale
                                text: qsTr("Ultimos 12 meses")
                                onClicked: root.applyLastTwelveMonths()
                            }
                            ActionButton {
                                objectName: "analyticsCustomCalendarToggle"
                                Layout.preferredWidth: 170 * root.compactControlScale
                                text: root.isoCalendarVisible ? qsTr("Ocultar calendario") : qsTr("Mostrar calendario")
                                onClicked: root.toggleIsoCalendar()
                            }
                        }
                    }

                    Label {
                        objectName: "analyticsCustomIsoRangeSummary"
                        Layout.fillWidth: true
                        visible: root.periodScope === 2 || root.periodScope === 3
                        text: qsTr("Mes ISO de referencia: inicio %1; fim %2. Cada semana pertence ao mes de sua quinta-feira.").arg(root.isoWeekLabel(root.firstYear, root.firstWeek)).arg(root.isoWeekLabel(root.lastYear, root.lastWeek))
                        color: Theme.mutedText
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        id: isoMonthCalendar

                        objectName: "analyticsIsoMonthCalendar"
                        property var months: root.analyticsViewModel.isoYearCalendar(root.periodYear)

                        Layout.fillWidth: true
                        Layout.preferredHeight: isoCalendarColumn.implicitHeight + Theme.cardGap * 2
                        visible: root.isoCalendarVisible
                        color: Theme.panelRaised
                        border.color: Theme.border
                        radius: Theme.radiusSoft

                        ColumnLayout {
                            id: isoCalendarColumn

                            anchors.fill: parent
                            anchors.margins: Theme.cardGap
                            spacing: Theme.gap

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Calendario SOM/ISO %1").arg(root.periodYear)
                                color: Theme.text
                                font.bold: true
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 3
                                columnSpacing: Theme.cardGap

                                Label {
                                    Layout.row: 0
                                    Layout.column: 0
                                    Layout.fillWidth: true
                                    text: qsTr("Mes ISO")
                                    color: Theme.mutedText
                                    font.bold: true
                                }
                                Label {
                                    Layout.row: 0
                                    Layout.column: 1
                                    text: qsTr("Primeira semana")
                                    color: Theme.mutedText
                                    font.bold: true
                                }
                                Label {
                                    Layout.row: 0
                                    Layout.column: 2
                                    text: qsTr("Ultima semana")
                                    color: Theme.mutedText
                                    font.bold: true
                                }
                                Repeater {
                                    model: isoMonthCalendar.months

                                    delegate: Label {
                                        required property int index
                                        required property var modelData

                                        objectName: "analyticsIsoMonthCalendarRow-" + String(modelData.month)
                                        Layout.row: index + 1
                                        Layout.column: 0
                                        Layout.fillWidth: true
                                        text: root.monthNames[modelData.month - 1] + "/" + String(modelData.year)
                                        color: Theme.text
                                    }
                                }
                                Repeater {
                                    model: isoMonthCalendar.months

                                    delegate: Label {
                                        required property int index
                                        required property var modelData

                                        objectName: "analyticsIsoMonthCalendarFirstWeek-" + String(modelData.month)
                                        Layout.row: index + 1
                                        Layout.column: 1
                                        text: modelData.firstWeekLabel
                                        color: Theme.text
                                    }
                                }
                                Repeater {
                                    model: isoMonthCalendar.months

                                    delegate: Label {
                                        required property int index
                                        required property var modelData

                                        objectName: "analyticsIsoMonthCalendarLastWeek-" + String(modelData.month)
                                        Layout.row: index + 1
                                        Layout.column: 2
                                        text: modelData.lastWeekLabel
                                        color: Theme.text
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Semanas SOM: segunda a domingo; o mes ISO e definido pela quinta-feira. Feriados e dias-ponte SOM nao carregados.")
                                color: Theme.mutedText
                                wrapMode: Text.Wrap
                            }
                        }
                    }

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
                            onValueModified: {
                                root.firstYear = value;
                                root.onPeriodEdited();
                            }
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
                            onValueModified: {
                                root.firstWeek = value;
                                root.onPeriodEdited();
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
                            value: root.lastYear
                            editable: true
                            onValueModified: {
                                root.lastYear = value;
                                root.onPeriodEdited();
                            }
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
                            onValueModified: {
                                root.lastWeek = value;
                                root.onPeriodEdited();
                            }
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
                            onActivated: root.invalidateCustomAnalysis()
                        }
                        Label {
                            text: qsTr("Quebra")
                            color: Theme.text
                        }
                        AppComboBox {
                            id: breakdownCombo

                            Layout.fillWidth: true
                            model: [qsTr("Divisao"), qsTr("Divisao e setor"), qsTr("Divisao e pessoa"), qsTr("Divisao, setor e pessoa")]
                            onActivated: root.invalidateCustomAnalysis()
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
                        Label {
                            text: qsTr("Ordenacao")
                            color: Theme.text
                        }
                        AppComboBox {
                            id: sortCombo

                            objectName: "analyticsCategorySort"
                            Layout.fillWidth: true
                            model: [qsTr("Ordem padrao"), qsTr("Setor alfabetico")]
                            onActivated: root.invalidateCustomChart()
                        }
                    }

                    ColumnLayout {
                        id: customActionControls

                        objectName: "analyticsCustomActionControls"
                        Layout.fillWidth: true
                        spacing: root.compactControlSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: root.compactControlSpacing

                            Label {
                                text: qsTr("Janela de alerta em dias")
                                color: Theme.text
                            }
                            AppTextField {
                                id: customWarningField

                                objectName: "analyticsCustomWarningField"

                                Layout.preferredWidth: 100 * root.compactControlScale
                                placeholderText: qsTr("Sem valor")
                                validator: IntValidator {
                                    bottom: 0
                                    top: 365
                                }
                                inputMethodHints: Qt.ImhDigitsOnly
                                Accessible.name: qsTr("Janela de alerta em dias da analise customizada")
                            }
                            ActionButton {
                                objectName: "analyticsRefreshOptions"
                                Layout.preferredWidth: Math.max(implicitWidth, 160 * root.compactControlScale)
                                text: qsTr("Atualizar opcoes")
                                enabled: !root.analyticsViewModel.loading
                                onClicked: root.refreshDimensions()
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Relatorios rapidos")
                            color: Theme.mutedText
                        }

                        Flow {
                            id: customQuickPresetFlow

                            objectName: "analyticsCustomQuickPresetFlow"
                            Layout.fillWidth: true
                            spacing: root.compactControlSpacing

                            ActionButton {
                                objectName: "analyticsOverdueByArea"
                                width: Math.max(implicitWidth, 160 * root.compactControlScale)
                                text: qsTr("Prazo das pendentes por area")
                                secondary: true
                                enabled: !root.analyticsViewModel.loading
                                onClicked: root.runQuickPreset(root.configureOverdueByArea)
                            }
                            ActionButton {
                                objectName: "analyticsExecutedByPerson"
                                width: Math.max(implicitWidth, 180 * root.compactControlScale)
                                text: qsTr("Executadas por pessoa")
                                secondary: true
                                enabled: !root.analyticsViewModel.loading
                                onClicked: root.runQuickPreset(root.configureExecutedByPerson)
                            }
                            ActionButton {
                                objectName: "analyticsExecutedBySectorWeek"
                                width: Math.max(implicitWidth, 190 * root.compactControlScale)
                                text: qsTr("Executadas/setor semana")
                                secondary: true
                                enabled: !root.analyticsViewModel.loading
                                onClicked: root.runQuickPreset(root.configureExecutedBySectorWeek)
                            }
                            ActionButton {
                                objectName: "analyticsExecutedBySectorMonth"
                                width: Math.max(implicitWidth, 190 * root.compactControlScale)
                                text: qsTr("Executadas/setor mes ISO")
                                secondary: true
                                enabled: !root.analyticsViewModel.loading
                                onClicked: root.runQuickPreset(root.configureExecutedBySectorMonth)
                            }
                            ActionButton {
                                objectName: "analyticsExecutedBySectorPerson"
                                width: Math.max(implicitWidth, 190 * root.compactControlScale)
                                text: qsTr("Executadas setor/pessoa")
                                secondary: true
                                enabled: !root.analyticsViewModel.loading
                                onClicked: root.runQuickPreset(root.configureExecutedBySectorPerson)
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: root.compactControlSpacing

                            AppCheckBox {
                                id: hideZeroCheck

                                objectName: "analyticsHideZeroCategories"
                                width: Math.max(hideZeroCheck.implicitWidth, 230 * root.compactControlScale)
                                text: qsTr("Ocultar categorias sem ocorrencias")
                                checked: root.hideZeroCategories
                                onToggled: root.hideZeroCategories = hideZeroCheck.checked
                            }
                            AppCheckBox {
                                id: listZeroCheck

                                objectName: "analyticsListZeroValues"
                                width: Math.max(listZeroCheck.implicitWidth, 180 * root.compactControlScale)
                                text: qsTr("Listar valores zero")
                                checked: root.listZeroValues
                                onToggled: root.listZeroValues = listZeroCheck.checked
                            }
                            Label {
                                visible: root.hideZeroCategories && customChart.hiddenZeroCategoryCount > 0
                                text: qsTr("%1 categoria(s) zero ocultada(s)").arg(customChart.hiddenZeroCategoryCount)
                                color: Theme.mutedText
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: root.compactControlSpacing

                            Item {
                                Layout.fillWidth: true
                            }
                            ActionButton {
                                objectName: "analyticsGenerateChart"
                                text: qsTr("Gerar grafico")
                                enabled: root.canAnalyze
                                onClicked: root.runAnalysis()
                            }
                            ActionButton {
                                objectName: "analyticsCancelCustomAnalysis"
                                text: qsTr("Cancelar")
                                enabled: root.analyticsViewModel.loading
                                danger: true
                                onClicked: root.analyticsViewModel.cancel()
                            }
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
                    Layout.preferredHeight: 216
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
                        AppTextField {
                            objectName: "analyticsDivisionSearch"
                            Layout.fillWidth: true
                            placeholderText: qsTr("Pesquisar codigo, nome ou departamento")
                            text: root.divisionSearchText
                            onTextEdited: root.divisionSearchText = text
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.gap

                            ActionButton {
                                objectName: "analyticsSelectAllDivisions"
                                Layout.preferredWidth: 130
                                text: qsTr("Selecionar todas")
                                onClicked: root.selectAllDivisions()
                            }
                            ActionButton {
                                objectName: "analyticsClearDivisions"
                                Layout.preferredWidth: 80
                                text: qsTr("Limpar")
                                onClicked: root.clearDivisions()
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }
                        ListView {
                            objectName: "analyticsDivisionList"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.filteredDimensionList("divisions", root.divisionSearchText)
                            ScrollBar.vertical: ScrollBar {}

                            delegate: SelectionOption {
                                required property var modelData
                                value: String(modelData)
                                displayText: root.organizationalUnitLabel(value)
                                optionObjectName: "analyticsDivisionOption-" + value
                                checked: root.isSelected(root.selectedDivisions, String(modelData))
                                onToggled: selected => {
                                    root.selectedDivisions = root.toggleValue(root.selectedDivisions, String(modelData), selected);
                                    root.selectedSectors = [];
                                    root.selectedPeople = [];
                                    root.invalidateCustomAnalysis();
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 216
                    color: Theme.panel
                    border.color: Theme.border
                    radius: Theme.radius
                    opacity: root.usesSectorSelection ? 1.0 : 0.55
                    enabled: root.usesSectorSelection

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.gap

                        Label {
                            text: qsTr("Setores")
                            color: Theme.text
                            font.bold: true
                        }
                        AppTextField {
                            objectName: "analyticsSectorSearch"
                            Layout.fillWidth: true
                            placeholderText: qsTr("Pesquisar codigo, nome ou departamento")
                            text: root.sectorSearchText
                            onTextEdited: root.sectorSearchText = text
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: !root.usesSectorSelection
                            text: qsTr("Ignorado nesta quebra (nao filtra a serie).")
                            color: Theme.mutedText
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.gap
                            visible: root.usesSectorSelection

                            ActionButton {
                                objectName: "analyticsSelectAllSectors"
                                Layout.preferredWidth: 130
                                text: qsTr("Selecionar todas")
                                onClicked: root.selectAllSectors()
                            }
                            ActionButton {
                                objectName: "analyticsClearSectors"
                                Layout.preferredWidth: 80
                                text: qsTr("Limpar")
                                onClicked: root.clearSectors()
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }
                        ListView {
                            objectName: "analyticsSectorList"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            enabled: root.usesSectorSelection
                            model: root.filteredDimensionList("sectors", root.sectorSearchText)
                            ScrollBar.vertical: ScrollBar {}

                            delegate: SelectionOption {
                                required property var modelData
                                value: String(modelData)
                                displayText: root.organizationalUnitLabel(value)
                                optionObjectName: "analyticsSectorOption-" + value
                                checked: root.isSelected(root.selectedSectors, String(modelData))
                                onToggled: selected => {
                                    root.selectedSectors = root.toggleValue(root.selectedSectors, String(modelData), selected);
                                    root.selectedPeople = [];
                                    root.invalidateCustomAnalysis();
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 216
                    color: Theme.panel
                    border.color: Theme.border
                    radius: Theme.radius
                    opacity: root.usesPeopleSelection ? 1.0 : 0.55
                    enabled: root.usesPeopleSelection

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
                            visible: !root.usesPeopleSelection
                            text: qsTr("Ignorado nesta quebra (nao filtra a serie).")
                            color: Theme.mutedText
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.gap
                            visible: root.usesPeopleSelection

                            ActionButton {
                                objectName: "analyticsSelectAllPeople"
                                Layout.preferredWidth: 130
                                text: qsTr("Selecionar todas")
                                onClicked: root.selectAllPeople()
                            }
                            ActionButton {
                                objectName: "analyticsClearPeople"
                                Layout.preferredWidth: 80
                                text: qsTr("Limpar")
                                onClicked: root.clearPeople()
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: root.usesPeopleSelection && root.dimensionList("people").length === 0
                            text: qsTr("Nenhuma pessoa disponivel para a selecao atual")
                            color: Theme.mutedText
                            wrapMode: Text.Wrap
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            enabled: root.usesPeopleSelection
                            model: root.dimensionList("people")
                            ScrollBar.vertical: ScrollBar {}

                            delegate: SelectionOption {
                                required property var modelData
                                value: String(modelData)
                                optionObjectName: "analyticsPersonOption-" + value
                                checked: root.isSelected(root.selectedPeople, String(modelData))
                                onToggled: selected => {
                                    root.selectedPeople = root.toggleValue(root.selectedPeople, String(modelData), selected);
                                    root.invalidateCustomChart();
                                }
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
                Layout.preferredHeight: customChart.implicitHeight
                objectName: "customAnalysisChart"
                title: root.chartTitle
                chartType: root.metricIndex === 8 ? "stackedBar" : "bar"
                chartModel: root.analyticsViewModel.customSeries
                hideZeroCategories: root.hideZeroCategories
                showExportActions: customChart.hasData
                fileWriter: (path, content) => root.analyticsViewModel.writeExportFile(path, content)
                itemGrabber: (item, path) => ChartImageExport.grabItemToFile(item, path)
                svgGrabber: (item, path) => ChartImageExport.grabItemToSvgFile(item, path)

                onExportCsvRequested: chartCsvExportDialog.open()
                onExportPngRequested: chartPngExportDialog.open()
                onExportSvgRequested: chartSvgExportDialog.open()
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.gap
                Layout.rightMargin: Theme.gap
                Layout.bottomMargin: Theme.gap
                Layout.preferredHeight: zeroValuesColumn.implicitHeight + Theme.cardGap * 2
                visible: root.listZeroValues
                color: Theme.panel
                border.color: Theme.border
                radius: Theme.radiusSoft

                ColumnLayout {
                    id: zeroValuesColumn

                    anchors.fill: parent
                    anchors.margins: Theme.cardGap
                    spacing: Theme.gap

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            Layout.fillWidth: true
                            text: customChart.zeroRows.length > 0 ? qsTr("Valores zero encontrados: %1").arg(customChart.zeroRows.length) : qsTr("Nenhum valor zero encontrado nos dados exibidos")
                            color: Theme.text
                            font.bold: true
                            wrapMode: Text.Wrap
                        }
                        ActionButton {
                            objectName: "analyticsExportZeroCsv"
                            text: qsTr("Exportar zeros em CSV")
                            enabled: customChart.zeroRows.length > 0
                            onClicked: zeroValuesCsvExportDialog.open()
                        }
                    }

                    AnalyticsChartTable {
                        id: zeroValuesTable

                        objectName: "analyticsZeroValuesTable"
                        Layout.fillWidth: true
                        Layout.preferredHeight: zeroValuesTable.implicitHeight
                        visible: customChart.zeroRows.length > 0
                        categoryHeader: qsTr("Categoria")
                        headers: [qsTr("Serie"), qsTr("Valor")]
                        rows: customChart.zeroRows
                    }
                }
            }
        }
    }

    FileDialog {
        id: chartCsvExportDialog

        objectName: "analyticsChartCsvExportDialog"
        title: qsTr("Exportar tabela como CSV")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [qsTr("CSV (*.csv)")]
        onAccepted: customChart.saveCsv(selectedFile)
    }

    FileDialog {
        id: chartPngExportDialog

        objectName: "analyticsChartPngExportDialog"
        title: qsTr("Exportar grafico como PNG")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "png"
        nameFilters: [qsTr("PNG (*.png)")]
        onAccepted: customChart.savePng(selectedFile)
    }

    FileDialog {
        id: chartSvgExportDialog

        objectName: "analyticsChartSvgExportDialog"
        title: qsTr("Exportar grafico como SVG (imagem embutida)")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "svg"
        nameFilters: [qsTr("SVG (*.svg)")]
        onAccepted: customChart.saveSvg(selectedFile)
    }

    FileDialog {
        id: zeroValuesCsvExportDialog

        objectName: "analyticsZeroValuesCsvExportDialog"
        title: qsTr("Exportar lista de zeros como CSV")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [qsTr("CSV (*.csv)")]
        onAccepted: customChart.saveCsvRows(selectedFile, qsTr("Categoria"), [qsTr("Serie"), qsTr("Valor")], customChart.zeroRows)
    }
}
