#include "presentation/FilterPanelViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <utility>
#include <vector>

namespace ssa::presentation {

    namespace {

        constexpr std::string_view kStatusExclusionSummary = "sem SCA/SES/STE";

        std::optional<int> parsePositiveInt(const QString& value) {
            const auto text = value.trimmed();
            if (text.isEmpty()) {
                return std::nullopt;
            }
            bool ok = false;
            const int parsed = text.toInt(&ok);
            if (!ok || parsed <= 0) {
                return std::nullopt;
            }
            return parsed;
        }

        domain::DerivationFilterMode derivationModeFromString(const QString& value) {
            if (value == "root") {
                return domain::DerivationFilterMode::RootOnly;
            }
            if (value == "derived") {
                return domain::DerivationFilterMode::DerivedOnly;
            }
            return domain::DerivationFilterMode::All;
        }

        std::string derivationModeToString(const domain::DerivationFilterMode mode) {
            if (mode == domain::DerivationFilterMode::RootOnly) {
                return "root";
            }
            if (mode == domain::DerivationFilterMode::DerivedOnly) {
                return "derived";
            }
            return "all";
        }

        std::vector<std::string>
        filterSummaryParts(const std::string_view quickSector, const bool excludeScaSesSte,
                           const std::map<std::string, std::string>& columnFilters,
                           const domain::AdvancedFilterSpec& advanced) {
            std::vector<std::string> parts;
            if (!quickSector.empty()) {
                parts.push_back(std::string(domain::ColumnCatalog::executorColumnKey()) + ":" +
                                std::string(quickSector));
            }
            if (excludeScaSesSte) {
                parts.push_back(std::string(kStatusExclusionSummary));
            }
            for (const auto& [key, value] : columnFilters) {
                parts.push_back(key + ":" + value);
            }
            if (advanced.year.has_value()) {
                parts.push_back("ano:" + std::to_string(*advanced.year));
            }
            if (advanced.week.has_value()) {
                parts.push_back("semana:" + std::to_string(*advanced.week));
            }
            if (advanced.derivationMode == domain::DerivationFilterMode::RootOnly) {
                parts.push_back("somente originais");
            } else if (advanced.derivationMode == domain::DerivationFilterMode::DerivedOnly) {
                parts.push_back("somente derivadas");
            }
            if (advanced.onlyReprogrammed) {
                parts.push_back("somente reprogramadas");
            }
            return parts;
        }

        std::string joinFilterSummary(const std::vector<std::string>& parts,
                                      const std::string_view separator = "  | ") {
            std::string summary;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) {
                    summary += separator;
                }
                summary += parts[i];
            }
            return summary;
        }

    } // namespace

    FilterPanelViewModel::FilterPanelViewModel(QObject* parent) : QObject(parent) {
        for (const auto& key : domain::ColumnCatalog::filterColumnKeys()) {
            filterColumnKeys_.push_back(QString::fromStdString(key));
        }
        for (const auto key : domain::ColumnCatalog::weekColumnKeys()) {
            weekColumnKeys_.push_back(
                QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size())));
        }
        columnKey_ = QString::fromStdString(domain::ColumnCatalog::defaultFilterColumnKey());
        weekColumnKey_ = "semana_programada";
        refreshActiveFilters();
    }

    QString FilterPanelViewModel::quickSector() const {
        return quickSector_;
    }

    void FilterPanelViewModel::setQuickSector(const QString& value) {
        const auto trimmed = value.trimmed();
        if (quickSector_ == trimmed) {
            return;
        }
        quickSector_ = trimmed;
        refreshActiveFilters();
        emit changed();
    }

    bool FilterPanelViewModel::excludeScaSesSte() const {
        return excludeScaSesSte_;
    }

    void FilterPanelViewModel::setExcludeScaSesSte(const bool value) {
        if (excludeScaSesSte_ == value) {
            return;
        }
        excludeScaSesSte_ = value;
        refreshActiveFilters();
        emit changed();
    }

    QStringList FilterPanelViewModel::filterColumnKeys() const {
        return filterColumnKeys_;
    }

    QString FilterPanelViewModel::columnKey() const {
        return columnKey_;
    }

    void FilterPanelViewModel::setColumnKey(const QString& value) {
        if (columnKey_ == value) {
            return;
        }
        columnKey_ = value;
        emit changed();
    }

    QString FilterPanelViewModel::columnValue() const {
        return columnValue_;
    }

    void FilterPanelViewModel::setColumnValue(const QString& value) {
        if (columnValue_ == value) {
            return;
        }
        columnValue_ = value;
        emit changed();
    }

    QStringList FilterPanelViewModel::weekColumnKeys() const {
        return weekColumnKeys_;
    }

    QString FilterPanelViewModel::weekColumnKey() const {
        return weekColumnKey_;
    }

    void FilterPanelViewModel::setWeekColumnKey(const QString& value) {
        const auto trimmed = value.trimmed();
        if (weekColumnKey_ == trimmed || !weekColumnKeys_.contains(trimmed)) {
            return;
        }
        weekColumnKey_ = trimmed;
        refreshActiveFilters();
        emit changed();
    }

    QString FilterPanelViewModel::yearFilter() const {
        return yearFilter_;
    }

    void FilterPanelViewModel::setYearFilter(const QString& value) {
        const auto trimmed = value.trimmed();
        if (yearFilter_ == trimmed) {
            return;
        }
        yearFilter_ = trimmed;
        refreshActiveFilters();
        emit changed();
    }

    QString FilterPanelViewModel::weekFilter() const {
        return weekFilter_;
    }

    void FilterPanelViewModel::setWeekFilter(const QString& value) {
        const auto trimmed = value.trimmed();
        if (weekFilter_ == trimmed) {
            return;
        }
        weekFilter_ = trimmed;
        refreshActiveFilters();
        emit changed();
    }

    QString FilterPanelViewModel::derivationMode() const {
        return derivationMode_;
    }

    void FilterPanelViewModel::setDerivationMode(const QString& value) {
        const auto trimmed = value.trimmed();
        if (derivationMode_ == trimmed) {
            return;
        }
        derivationMode_ = trimmed == "root" || trimmed == "derived" ? trimmed : "all";
        refreshActiveFilters();
        emit changed();
    }

    bool FilterPanelViewModel::onlyReprogrammed() const {
        return onlyReprogrammed_;
    }

    void FilterPanelViewModel::setOnlyReprogrammed(const bool value) {
        if (onlyReprogrammed_ == value) {
            return;
        }
        onlyReprogrammed_ = value;
        refreshActiveFilters();
        emit changed();
    }

    QStringList FilterPanelViewModel::activeFilters() const {
        return activeFilters_;
    }

    QString FilterPanelViewModel::activeFilterSummary() const {
        return activeFilterSummary_;
    }

    std::map<std::string, std::string> FilterPanelViewModel::columnFilters() const {
        return columnFilters_;
    }

    domain::AdvancedFilterSpec FilterPanelViewModel::advancedFilters() const {
        domain::AdvancedFilterSpec filters;
        filters.weekColumnKey = weekColumnKey_.trimmed().toStdString();
        filters.year = parsePositiveInt(yearFilter_);
        filters.week = parsePositiveInt(weekFilter_);
        filters.derivationMode = derivationModeFromString(derivationMode_);
        filters.onlyReprogrammed = onlyReprogrammed_;
        return filters;
    }

    bool FilterPanelViewModel::hasFilterForColumn(const QString& key) const {
        const auto normalizedKey = key.trimmed().toStdString();
        if (domain::ColumnCatalog::isQuickSectorFilterColumn(normalizedKey) &&
            !quickSector_.trimmed().isEmpty()) {
            return true;
        }
        if (domain::ColumnCatalog::isStatusExclusionFilterColumn(normalizedKey) &&
            excludeScaSesSte_) {
            return true;
        }
        if (normalizedKey == weekColumnKey_.toStdString() &&
            (parsePositiveInt(yearFilter_).has_value() ||
             parsePositiveInt(weekFilter_).has_value())) {
            return true;
        }
        if (normalizedKey == domain::ColumnCatalog::derivationColumnKey() &&
            derivationMode_ != "all") {
            return true;
        }
        for (const auto key : domain::ColumnCatalog::reprogrammingColumnKeys()) {
            if (normalizedKey == key && onlyReprogrammed_) {
                return true;
            }
        }
        return columnFilters_.contains(normalizedKey);
    }

    void FilterPanelViewModel::setColumnFilters(std::map<std::string, std::string> filters) {
        if (columnFilters_ == filters) {
            return;
        }
        columnFilters_ = std::move(filters);
        refreshActiveFilters();
        emit changed();
    }

    void FilterPanelViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        bool didChange = false;
        const auto nextQuickSector = QString::fromStdString(snapshot.quickSector).trimmed();
        if (quickSector_ != nextQuickSector) {
            quickSector_ = nextQuickSector;
            didChange = true;
        }
        if (excludeScaSesSte_ != snapshot.excludeScaSesSte) {
            excludeScaSesSte_ = snapshot.excludeScaSesSte;
            didChange = true;
        }
        if (columnFilters_ != snapshot.columnFilters) {
            columnFilters_ = snapshot.columnFilters;
            didChange = true;
        }
        const auto nextWeekColumnKey =
            QString::fromStdString(snapshot.advancedWeekColumnKey).trimmed();
        if (weekColumnKeys_.contains(nextWeekColumnKey) && weekColumnKey_ != nextWeekColumnKey) {
            weekColumnKey_ = nextWeekColumnKey;
            didChange = true;
        }
        const auto nextYear = QString::fromStdString(snapshot.advancedYear).trimmed();
        if (yearFilter_ != nextYear) {
            yearFilter_ = nextYear;
            didChange = true;
        }
        const auto nextWeek = QString::fromStdString(snapshot.advancedWeek).trimmed();
        if (weekFilter_ != nextWeek) {
            weekFilter_ = nextWeek;
            didChange = true;
        }
        const auto nextDerivation = QString::fromStdString(snapshot.derivationMode).trimmed();
        if (derivationMode_ != nextDerivation) {
            derivationMode_ =
                nextDerivation == "root" || nextDerivation == "derived" ? nextDerivation : "all";
            didChange = true;
        }
        if (onlyReprogrammed_ != snapshot.onlyReprogrammed) {
            onlyReprogrammed_ = snapshot.onlyReprogrammed;
            didChange = true;
        }
        if (!didChange) {
            return;
        }
        refreshActiveFilters();
        emit changed();
    }

    void FilterPanelViewModel::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        snapshot.quickSector = quickSector_.trimmed().toStdString();
        snapshot.excludeScaSesSte = excludeScaSesSte_;
        snapshot.columnFilters = columnFilters_;
        snapshot.advancedWeekColumnKey = weekColumnKey_.trimmed().toStdString();
        snapshot.advancedYear = yearFilter_.trimmed().toStdString();
        snapshot.advancedWeek = weekFilter_.trimmed().toStdString();
        snapshot.derivationMode = derivationMode_.trimmed().toStdString();
        snapshot.onlyReprogrammed = onlyReprogrammed_;
    }

    void FilterPanelViewModel::addColumnFilter() {
        const auto key = columnKey_.trimmed();
        const auto value = columnValue_.trimmed();
        if (!key.isEmpty() && !value.isEmpty()) {
            auto& filter = columnFilters_[key.toStdString()];
            if (filter.empty()) {
                filter = value.toStdString();
            } else {
                filter += ", " + value.toStdString();
            }
            columnValue_.clear();
            refreshActiveFilters();
            emit changed();
            emit applyRequested();
        }
    }

    void FilterPanelViewModel::resetFilters() {
        quickSector_.clear();
        columnKey_ = QString::fromStdString(domain::ColumnCatalog::defaultFilterColumnKey());
        columnValue_.clear();
        weekColumnKey_ = "semana_programada";
        yearFilter_.clear();
        weekFilter_.clear();
        derivationMode_ = "all";
        onlyReprogrammed_ = false;
        columnFilters_.clear();
        excludeScaSesSte_ = domain::kDefaultExcludeScaSesSte;
        refreshActiveFilters();
        emit changed();
        emit applyRequested();
    }

    void FilterPanelViewModel::rebuildActiveFilters() {
        const auto activeParts =
            filterSummaryParts(quickSector_.trimmed().toStdString(), excludeScaSesSte_,
                               columnFilters_, advancedFilters());
        activeFilters_.clear();
        activeFilters_.reserve(static_cast<int>(activeParts.size()));
        for (const auto& filter : activeParts) {
            activeFilters_.push_back(QString::fromStdString(filter));
        }
        activeFilterSummary_ = QString::fromStdString(joinFilterSummary(activeParts));
    }

    void FilterPanelViewModel::refreshActiveFilters() {
        rebuildActiveFilters();
    }

} // namespace ssa::presentation
