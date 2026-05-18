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

        QStringList toColumnValueDisplayList(const std::vector<std::string>& values) {
            QStringList priority;
            QStringList other;
            const auto capacity = static_cast<int>(values.size());
            priority.reserve(capacity);
            other.reserve(capacity);
            for (const auto& value : values) {
                if (domain::isPriorityColumnValue(value)) {
                    priority.append(QString::fromStdString(value));
                } else {
                    other.append(QString::fromStdString(value));
                }
            }
            for (auto& value : other) {
                priority.append(std::move(value));
            }
            return priority;
        }

        void trimColumnValueOptionCache(std::map<QString, ColumnValueOptionCacheEntry>& cache,
                                        const QString& protectedKey) {
            while (cache.size() > kMaxColumnValueOptionCacheEntries) {
                auto optionIt = cache.end();
                for (auto candidate = cache.begin(); candidate != cache.end(); ++candidate) {
                    if (candidate->first != protectedKey) {
                        optionIt = candidate;
                        break;
                    }
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
          columns_(state_, this), sector_(state_, this),
          distinctValues_(std::move(queryService), state_, this), activeFilterRefreshTimer_(this) {
        loadFilterCatalog();
        advanced_ = new FilterPanelAdvancedViewModel(state_.advanced(), weekColumnKeys_, this);
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
        sector_.setExcludeScaSesSte(value);
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

    void FilterPanelViewModel::setColumnValueOptions(const std::vector<std::string>& options,
                                                     const QString& key) {
        const auto normalizedKey = key.trimmed();
        columnValueLoadingKeys_.remove(normalizedKey);
        auto displayList = toColumnValueDisplayList(options);
        columnValueOptionsByKey_[normalizedKey] = {
            .source = options, .options = displayList, .previewSource = displayList};
        trimColumnValueOptionCache(columnValueOptionsByKey_, normalizedKey);
        ++columnValueOptionsVersion_;
        emit columnValueOptionsChanged();
    }

    void FilterPanelViewModel::publishFilterStateChange(const bool quickSectorChanged) {
        clearColumnValueOptionsCache();
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
        if (cached != columnValueOptionsByKey_.end()) {
            ++columnValueOptionsVersion_;
            emit columnValueOptionsChanged();
            return;
        }
        columnValueLoadingKeys_.insert(normalizedKey);
        ++columnValueOptionsVersion_;
        emit columnValueOptionsChanged();
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
        const auto activeParts =
            filterpanel::buildFilterSummaryParts(state_.quickSector().trimmed().toStdString(),
                                                 state_.columnFilters(), advancedFilters());
        QStringList nextActiveFilters;
        nextActiveFilters.reserve(static_cast<qsizetype>(activeParts.size()));
        for (const auto& filter : activeParts) {
            nextActiveFilters.append(QString::fromStdString(filter));
        }
        const auto nextSummary =
            QString::fromStdString(filterpanel::joinFilterSummary(activeParts, "  | "));
        if (activeFilters_ == nextActiveFilters && activeFilterSummary_ == nextSummary) {
            return;
        }
        activeFilters_ = std::move(nextActiveFilters);
        activeFilterSummary_ = nextSummary;
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

    void FilterPanelViewModel::clearColumnValueOptionsCache() {
        if (columnValueOptionsByKey_.empty() && columnValueLoadingKeys_.empty()) {
            return;
        }
        columnValueOptionsByKey_.clear();
        columnValueLoadingKeys_.clear();
        ++columnValueOptionsVersion_;
        emit columnValueOptionsChanged();
    }

    void FilterPanelViewModel::scheduleActiveFilterRefresh() {
        markActiveFiltersDirty();
        activeFilterRefreshTimer_.start();
    }

    void FilterPanelViewModel::markActiveFiltersDirty() {
        activeFiltersDirty_ = true;
    }

} // namespace ssa::presentation
