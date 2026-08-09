import QtQuick
import SsaConsultaRapida

AnalyticsChart {
    id: root

    property var chartModel: ({})
    property bool hideZeroCategories: false

    readonly property var categoryIndexes: visibleCategoryIndexes()
    readonly property var presentedCategories: presentCategories()
    readonly property var presentedSeries: presentSeries()
    readonly property var presentedTrendValues: presentTrendValues()
    readonly property var zeroRows: collectZeroRows()
    readonly property int hiddenZeroCategoryCount: Math.max(0, sourceCategories().length - presentedCategories.length)
    readonly property bool unavailable: chartModel && chartModel.available === false
    readonly property real preferredCardHeight: unavailable ? 140 : 380

    function seriesLabel(name) {
        const labels = {
            "total": qsTr("Total"),
            "registered_in_period": qsTr("Cadastradas no periodo"),
            "registered_before_period": qsTr("Cadastradas anteriormente"),
            "registration_unknown": qsTr("Cadastro sem data conhecida"),
            "on_time": qsTr("No prazo"),
            "warning": qsTr("Alerta"),
            "overdue": qsTr("Fora do prazo")
        };
        return labels[name] === undefined ? name : labels[name];
    }

    function seriesColor(name) {
        const colors = {
            "registered_in_period": "#4c78a8",
            "registered_before_period": "#d95f5f",
            "registration_unknown": "#9aa0a6",
            "on_time": "#2e7d32",
            "warning": "#f9a825",
            "overdue": "#c62828"
        };
        return colors[name];
    }

    function sourceCategories() {
        return root.chartModel && root.chartModel.categories ? root.chartModel.categories : [];
    }

    function sourceSeries() {
        return root.chartModel && root.chartModel.series ? root.chartModel.series : [];
    }

    function visibleCategoryIndexes() {
        const categories = root.sourceCategories();
        const series = root.sourceSeries();
        const indexes = [];
        for (let categoryIndex = 0; categoryIndex < categories.length; ++categoryIndex) {
            let allZero = root.hideZeroCategories && series.length > 0;
            for (let seriesIndex = 0; allZero && seriesIndex < series.length; ++seriesIndex) {
                const entry = series[seriesIndex];
                const values = entry && entry.values ? entry.values : [];
                const value = values[categoryIndex];
                allZero = typeof value === "number" && Number.isFinite(value) && value === 0;
            }
            if (!allZero)
                indexes.push(categoryIndex);
        }
        return indexes;
    }

    function filteredValues(values) {
        const output = [];
        for (let index = 0; index < root.categoryIndexes.length; ++index)
            output.push(values[root.categoryIndexes[index]]);
        return output;
    }

    function presentCategories() {
        return root.filteredValues(root.sourceCategories());
    }

    function presentTrendValues() {
        const input = root.chartModel && root.chartModel.trendValues ? root.chartModel.trendValues : [];
        if (input.length === 0)
            return [];
        if (Array.isArray(input[0])) {
            const output = [];
            for (let index = 0; index < input.length; ++index)
                output.push(root.filteredValues(input[index]));
            return output;
        }
        return root.filteredValues(input);
    }

    function collectZeroRows() {
        const categories = root.sourceCategories();
        const series = root.sourceSeries();
        const rows = [];
        for (let categoryIndex = 0; categoryIndex < categories.length; ++categoryIndex) {
            for (let seriesIndex = 0; seriesIndex < series.length; ++seriesIndex) {
                const entry = series[seriesIndex];
                const values = entry && entry.values ? entry.values : [];
                const value = values[categoryIndex];
                if (typeof value !== "number" || !Number.isFinite(value) || value !== 0)
                    continue;
                const name = entry && entry.name !== undefined ? String(entry.name) : "";
                rows.push({
                    "category": categories[categoryIndex],
                    "values": [root.seriesLabel(name), "0"]
                });
            }
        }
        return rows;
    }

    function presentSeries() {
        const input = root.sourceSeries();
        const output = [];
        for (let index = 0; index < input.length; ++index) {
            const item = input[index];
            const name = item && item.name !== undefined ? String(item.name) : "";
            const presented = {
                "key": name,
                "name": root.seriesLabel(name),
                "tag": item && item.tag !== undefined ? String(item.tag) : "",
                "values": root.filteredValues(item && item.values ? item.values : []),
                "trendValues": root.filteredValues(item && item.trendValues ? item.trendValues : [])
            };
            const color = root.seriesColor(name);
            if (color !== undefined)
                presented.color = color;
            output.push(presented);
        }
        return output;
    }

    function presentedQualityPart(part) {
        if (part.indexOf("excluded_for_data_quality=") === 0)
            return qsTr("Itens fora do denominador: %1").arg(part.split("=")[1]);
        if (part.indexOf("snapshot_stale_by_weeks=") === 0)
            return qsTr("Captura atrasada em %1 semana(s)").arg(part.split("=")[1]);
        return root.presentedUnavailableReason(part);
    }

    function presentedQualityText() {
        const text = root.chartModel && root.chartModel.qualityText ? String(root.chartModel.qualityText) : "";
        const parts = text.split(" | ");
        const presented = [];
        for (let index = 0; index < parts.length; ++index)
            presented.push(root.presentedQualityPart(parts[index]));
        return presented.join(" | ");
    }

    function presentedUnavailableReason(reason) {
        const labels = {
            "complete partial-attention source is unavailable": qsTr("fonte completa de atencao parcial nao disponivel"),
            "warning window is required for deadline analytics": qsTr("informe a janela de alerta para analisar prazos"),
            "analytics snapshot is incomplete": qsTr("captura analitica incompleta"),
            "snapshot history is unavailable": qsTr("historico de capturas indisponivel"),
            "analytics_result_incomplete": qsTr("resultado analitico incompleto"),
            "projection_unavailable": qsTr("projecao analitica indisponivel")
        };
        return labels[reason] === undefined ? reason : labels[reason];
    }

    function unavailableMessage() {
        if (!root.chartModel || root.chartModel.available !== false)
            return qsTr("Sem dados disponiveis para o periodo selecionado");
        const reason = root.chartModel.unavailableReason ? root.presentedUnavailableReason(String(root.chartModel.unavailableReason)) : qsTr("Fonte indisponivel");
        return qsTr("Indisponivel: %1").arg(reason);
    }

    categories: presentedCategories
    compact: unavailable
    series: presentedSeries
    trendValues: presentedTrendValues
    subtitle: chartModel && chartModel.subtitle ? String(chartModel.subtitle) : ""
    qualityText: presentedQualityText()
    emptyMessage: unavailableMessage()
    valueSuffix: chartType === "percentStackedBar" ? "%" : ""
    xAxisTitle: chartType === "trendLine" ? qsTr("Tempo") : qsTr("Categoria")
    yAxisTitle: chartType === "percentStackedBar" ? qsTr("Percentual") : qsTr("SSAs")
}
