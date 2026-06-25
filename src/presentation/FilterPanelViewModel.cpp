#include "presentation/FilterPanelViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/ColumnValuePriorityPolicy.h"
#include "presentation/FilterPanelStateHelpers.h"
#include "query/SsaQueryService.h"

#include <QVariantMap>

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace ssa::presentation {
    namespace {
        constexpr std::size_t kMaxColumnValueOptionCacheEntries = 24;

        QVariantMap summaryEntryMap(const filterpanel::FilterSummaryEntry& entry) {
            QVariantMap map;
            map.insert(QStringLiteral("text"), QString::fromStdString(entry.text));
            map.insert(QStringLiteral("kind"), QString::fromStdString(entry.kind));
            map.insert(QStringLiteral("key"), QString::fromStdString(entry.key));
            return map;
        }

        QStringList toColumnValueDisplayList(const std::vector<std::string>& values) {
            QStringList priorityValues;
            QStringList otherValues;
            const auto capacity = static_cast<int>(values.size());
            priorityValues.reserve(capacity);
            otherValues.reserve(capacity);
            for (const auto& value : values) {
                auto displayValue = QString::fromStdString(value);
                if (domain::isPriorityColumnValue(value)) {
                    priorityValues.append(std::move(displayValue));
                } else {
                    otherValues.append(std::move(displayValue));
                }
            }
            priorityValues.append(otherValues);
            return priorityValues;
        }

        void trimColumnValueOptionCache(std::map<QString, ColumnValueOptionCacheEntry>& cache,
                                        const QString& protectedKey) {
            while (cache.size() > kMaxColumnValueOptionCacheEntries) {
                auto optionIt = cache.begin();
                if (optionIt != cache.end() && optionIt->first == protectedKey) {
                    ++optionIt;
                }
                if (optionIt == cache.end()) {
                    return;
                }
                cache.erase(optionIt);
            }
        }

    } // namespace

    FilterPanelViewModel::FilterPanelViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                               QObject* parent)
        : QObject(parent), state_{domain::ColumnCatalog::defaultFilterColumnKey()},
          queryService_(std::move(queryService)), columns_(state_, this), sector_(state_, this),
          distinctValues_(queryService_, state_, this), activeFilterRefreshTimer_(this) {
        loadFilterCatalog();
        advanced_ = new FilterPanelAdvancedViewModel(state_.advanced(), state_, weekColumnKeys_,
                                                     queryService_, this);
        connect(advanced_, &FilterPanelAdvancedViewModel::stateChanged, this,
                [this]() { publishFilterStateChange(); });
        connect(advanced_, &FilterPanelAdvancedViewModel::applyRequested, this,
                &FilterPanelViewModel::applyRequested);
        connect(&columns_, &ColumnFilterViewModel::stateChanged, this,
                [this]() { synchronizeFilterState(false); });
        connect(&columns_, &ColumnFilterViewModel::applyRequested, this,
                &FilterPanelViewModel::applyRequested);
        configureDistinctValueRefresh();
        refreshActiveFilters();
        scheduleColumnValueRefresh();
        refreshQuickSectorOptions();
    }

    void FilterPanelViewModel::loadFilterCatalog() {
        for (const auto& key : domain::ColumnCatalog::orderedFilterColumnKeys()) {
            filterColumnKeys_.push_back(QString::fromStdString(key));
        }
        for (const auto key : domain::ColumnCatalog::weekColumnKeys()) {
            weekColumnKeys_.push_back(
                QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size())));
        }
    }

    void FilterPanelViewModel::configureDistinctValueRefresh() {
        activeFilterRefreshTimer_.setInterval(120);
        activeFilterRefreshTimer_.setSingleShot(true);
        connect(&activeFilterRefreshTimer_, &QTimer::timeout, this, [this]() {
            refreshActiveFilters();
            emit changed();
        });
        connect(&distinctValues_, &FilterPanelDistinctValuesController::columnValueOptionsReady,
                this, [this](const std::vector<std::string>& values, const QString& key) {
                    setColumnValueOptions(values, key);
                });
        connect(&distinctValues_, &FilterPanelDistinctValuesController::quickSectorOptionsReady,
                this,
                [this](const std::vector<std::string>& values) { sector_.setOptions(values); });
        connect(&sector_, &FilterPanelSectorViewModel::stateChanged, this,
                [this](const bool quickSectorChanged) {
                    publishFilterStateChange(quickSectorChanged);
                });
    }

    QString FilterPanelViewModel::quickSector() const {
        return sector_.quickSector();
    }

    void FilterPanelViewModel::setQuickSector(const QString& value) {
        sector_.setQuickSector(value);
    }

    bool FilterPanelViewModel::excludeScaSesSte() const {
        return sector_.excludeScaSesSte();
    }

    void FilterPanelViewModel::setExcludeScaSesSte(const bool value) {
        const bool changed = sector_.excludeScaSesSte() != value;
        if (!changed) {
            return;
        }
        sector_.setExcludeScaSesSte(value);
        publishFilterStateChange();
    }

    QStringList FilterPanelViewModel::filterColumnKeys() const {
        return filterColumnKeys_;
    }

    QString FilterPanelViewModel::columnKey() const {
        return state_.columnKey();
    }

    void FilterPanelViewModel::setColumnKey(const QString& value) {
        if (!state_.setColumnKey(value)) {
            return;
        }
        publishFilterStateChange();
    }

    QString FilterPanelViewModel::columnValue() const {
        return state_.columnValue();
    }

    void FilterPanelViewModel::setColumnValue(const QString& value) {
        if (!state_.setColumnValue(value)) {
            return;
        }
        publishFilterStateChange();
    }

    QStringList FilterPanelViewModel::weekColumnKeys() const {
        return weekColumnKeys_;
    }

    QObject* FilterPanelViewModel::advanced() const {
        return advanced_;
    }

    QObject* FilterPanelViewModel::columns() {
        return &columns_;
    }

    QObject* FilterPanelViewModel::sector() {
        return &sector_;
    }

    QStringList FilterPanelViewModel::activeFilters() const {
        return activeFilters_;
    }

    QString FilterPanelViewModel::activeFilterSummary() const {
        return activeFilterSummary_;
    }

    QVariantList FilterPanelViewModel::activeFilterEntries() const {
        return activeFilterEntries_;
    }

    std::map<std::string, std::string> FilterPanelViewModel::columnFilters() const {
        return state_.columnFilters();
    }

    domain::AdvancedFilterSpec FilterPanelViewModel::advancedFilters() const {
        return state_.advancedFilters();
    }

    bool FilterPanelViewModel::hasFilterForColumn(const QString& key) const {
        return state_.hasFilterForColumn(key);
    }

    int FilterPanelViewModel::columnValueOptionsVersion() const {
        return columnValueOptionsVersion_;
    }

    QStringList FilterPanelViewModel::quickSectorOptions() const {
        return sector_.options();
    }

    QStringList FilterPanelViewModel::quickSectorSelectorValues() const {
        return sector_.selectorValues();
    }

    int FilterPanelViewModel::quickSectorSelectorIndex() const {
        return sector_.selectorIndex();
    }

    QStringList FilterPanelViewModel::columnValueOptionsFor(const QString& key) const {
        const auto it = columnValueOptionsByKey_.find(key.trimmed());
        return it == columnValueOptionsByKey_.end() ? QStringList{} : it->second.options;
    }

    QStringList FilterPanelViewModel::columnValuePreviewOptionsFor(const QString& key,
                                                                   const int limit,
                                                                   const bool expanded) const {
        const auto it = columnValueOptionsByKey_.find(key.trimmed());
        const auto values =
            it == columnValueOptionsByKey_.end() ? QStringList{} : it->second.previewSource;
        // Expanded mode intentionally shows every loaded option for that column.
        if (expanded || limit <= 0 || values.size() <= limit) {
            return values;
        }
        return values.sliced(0, std::min(limit, static_cast<int>(values.size())));
    }

    bool FilterPanelViewModel::hasMoreColumnValueOptionsFor(const QString& key,
                                                            const int limit) const {
        return limit > 0 && columnValueOptionsFor(key).size() > limit;
    }

    bool FilterPanelViewModel::columnValueOptionsLoadingFor(const QString& key) const {
        return columnValueLoadingKeys_.contains(key.trimmed());
    }

    bool FilterPanelViewModel::removeActiveFilter(const QString& kind, const QString& key) {
        const auto action = kind.trimmed();
        bool changed = false;
        if (action == "quick_sector") {
            changed = state_.setQuickSector({});
            if (changed) {
                sector_.refreshFromState();
            }
        } else if (action == "column") {
            changed = state_.removeColumnFilter(key);
            if (changed) {
                columns_.refreshFromState();
            }
        } else if (action == "advanced_text") {
            changed = state_.advanced().setTextFilter(key, {});
        } else if (action == "advanced_year") {
            changed = state_.advanced().setYear({});
        } else if (action == "advanced_week") {
            changed = state_.advanced().setWeek({});
        } else if (action == "advanced_issue_year") {
            changed = state_.advanced().setIssueYear({});
        } else if (action == "advanced_execution_year") {
            changed = state_.advanced().setExecutionYear({});
        } else if (action == "advanced_reprogramming") {
            const bool equalsChanged = state_.advanced().setReprogrammingEquals({});
            const bool valuesChanged = state_.advanced().setReprogrammingValues({});
            changed = equalsChanged || valuesChanged;
        } else if (action == "advanced_issue_week_range") {
            const bool startChanged = state_.advanced().setIssueWeekStart({});
            const bool endChanged = state_.advanced().setIssueWeekEnd({});
            changed = startChanged || endChanged;
        } else if (action == "advanced_execution_week_range") {
            const bool startChanged = state_.advanced().setExecutionWeekStart({});
            const bool endChanged = state_.advanced().setExecutionWeekEnd({});
            changed = startChanged || endChanged;
        } else if (action == "advanced_derivation_mode") {
            changed = state_.advanced().setDerivationMode(QStringLiteral("all"));
        } else if (action == "advanced_only_reprogrammed") {
            changed = state_.advanced().setOnlyReprogrammed(false);
        }

        if (!changed) {
            return false;
        }
        advanced_->refreshFromState();
        synchronizeFilterState(action == "quick_sector");
        emit applyRequested();
        return true;
    }

    void FilterPanelViewModel::setColumnValueOptions(const std::vector<std::string>& options,
                                                     const QString& key) {
        const auto normalizedKey = key.trimmed();
        columnValueLoadingKeys_.remove(normalizedKey);
        auto displayList = toColumnValueDisplayList(options);
        columnValueOptionsByKey_[normalizedKey] = {options, displayList, displayList,
                                                   filterStateVersion_};
        trimColumnValueOptionCache(columnValueOptionsByKey_, normalizedKey);
        ++columnValueOptionsVersion_;
        emit columnValueOptionsChanged();
        emit columnValueOptionsChangedFor(normalizedKey);
    }

    void FilterPanelViewModel::publishFilterStateChange(const bool quickSectorChanged) {
        ++filterStateVersion_;
        columnValueLoadingKeys_.clear();
        scheduleActiveFilterRefresh();
        scheduleColumnValueRefresh();
        if (!quickSectorChanged) {
            refreshQuickSectorOptions();
        }
    }

    void FilterPanelViewModel::setColumnFilters(std::map<std::string, std::string> filters) {
        if (!state_.setColumnFilters(std::move(filters))) {
            return;
        }
        columns_.refreshFromState();
        synchronizeFilterState(false);
    }

    void FilterPanelViewModel::refreshColumnValueOptions() {
        refreshColumnValueOptionsFor(state_.columnKey());
    }

    void FilterPanelViewModel::refreshColumnValueOptionsFor(const QString& key) {
        const auto normalizedKey = key.trimmed();
        if (columnValueLoadingKeys_.contains(normalizedKey)) {
            return;
        }
        const auto cached = columnValueOptionsByKey_.find(normalizedKey);
        if (cached != columnValueOptionsByKey_.end() &&
            cached->second.stateVersion == filterStateVersion_) {
            ++columnValueOptionsVersion_;
            emit columnValueOptionsChanged();
            emit columnValueOptionsChangedFor(normalizedKey);
            return;
        }
        columnValueLoadingKeys_.insert(normalizedKey);
        ++columnValueOptionsVersion_;
        emit columnValueOptionsChanged();
        emit columnValueOptionsChangedFor(normalizedKey);
        distinctValues_.refreshColumnValueOptionsFor(normalizedKey);
    }

    void FilterPanelViewModel::refreshQuickSectorOptions() {
        distinctValues_.refreshQuickSectorOptions();
    }

    void FilterPanelViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        if (!state_.applyPreferences(snapshot, weekColumnKeys_)) {
            return;
        }
        sector_.refreshFromState();
        advanced_->refreshFromState();
        columns_.refreshFromState();
        synchronizeFilterState(true);
    }

    void FilterPanelViewModel::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        state_.writePreferences(snapshot);
    }

    void FilterPanelViewModel::addColumnFilter() {
        if (!columns_.applyFilterFor(state_.columnKey(), state_.columnValue())) {
            return;
        }
        state_.setColumnValue(QString{});
    }

    void FilterPanelViewModel::resetFilters() {
        state_.clear();
        sector_.refreshFromState();
        advanced_->refreshFromState();
        columns_.refreshFromState();
        synchronizeFilterState(true);
        emit applyRequested();
    }

    void FilterPanelViewModel::rebuildActiveFilters() {
        if (!activeFiltersDirty_) {
            return;
        }
        activeFiltersDirty_ = false;
        const auto activeEntries =
            filterpanel::buildFilterSummaryEntries(state_.quickSector().trimmed().toStdString(),
                                                   state_.columnFilters(), advancedFilters());
        std::vector<std::string> activeParts;
        activeParts.reserve(activeEntries.size());
        QStringList nextActiveFilters;
        QVariantList nextActiveFilterEntries;
        nextActiveFilters.reserve(static_cast<qsizetype>(activeEntries.size()));
        nextActiveFilterEntries.reserve(static_cast<qsizetype>(activeEntries.size()));
        for (const auto& entry : activeEntries) {
            activeParts.push_back(entry.text);
            nextActiveFilters.append(QString::fromStdString(entry.text));
            nextActiveFilterEntries.append(summaryEntryMap(entry));
        }
        const auto nextSummary =
            QString::fromStdString(filterpanel::joinFilterSummary(activeParts, "  | "));
        if (activeFilters_ == nextActiveFilters && activeFilterSummary_ == nextSummary &&
            activeFilterEntries_ == nextActiveFilterEntries) {
            return;
        }
        activeFilters_ = std::move(nextActiveFilters);
        activeFilterSummary_ = nextSummary;
        activeFilterEntries_ = std::move(nextActiveFilterEntries);
    }

    void FilterPanelViewModel::refreshActiveFilters() {
        rebuildActiveFilters();
    }

    void FilterPanelViewModel::synchronizeFilterState(const bool refreshSectorOptions) {
        markActiveFiltersDirty();
        refreshActiveFilters();
        scheduleColumnValueRefresh();
        if (refreshSectorOptions) {
            refreshQuickSectorOptions();
        }
        emit changed();
    }

    void FilterPanelViewModel::scheduleColumnValueRefresh() {
        distinctValues_.scheduleColumnValueRefresh();
    }

    void FilterPanelViewModel::scheduleActiveFilterRefresh() {
        markActiveFiltersDirty();
        activeFilterRefreshTimer_.start();
    }

    void FilterPanelViewModel::markActiveFiltersDirty() {
        activeFiltersDirty_ = true;
    }

} // namespace ssa::presentation
