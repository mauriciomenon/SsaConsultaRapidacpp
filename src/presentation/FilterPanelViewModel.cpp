#include "presentation/FilterPanelViewModel.h"

#include "domain/ColumnCatalog.h"
#include "presentation/FilterPanelCanonicalizer.h"
#include "presentation/FilterPanelStateHelpers.h"
#include "query/SsaQueryService.h"
#include "query/TextFilterToken.h"

#include <QVariantMap>

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ssa::presentation {
    namespace {
        constexpr int kActiveFilterRefreshDelayMs = 120;
        constexpr std::array<std::string_view, 26> kStatusShortcutValues{{
            "AAD", "AAT", "ACC", "ACS", "ADI", "ADM", "AIM", "ALE", "AMP",
            "APG", "APL", "APV", "ASE", "ASL", "ASO", "SAD", "SAS", "SCA",
            "SCC", "SCD", "SCS", "SEE", "SES", "SPG", "SRP", "STE",
        }};

        QString statusColumnKey() {
            const auto key = domain::ColumnCatalog::statusColumnKey();
            return QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
        }

        QString executorColumnKey() {
            const auto key = domain::ColumnCatalog::executorColumnKey();
            return QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
        }

        QVariantMap summaryEntryMap(const filterpanel::FilterSummaryEntry& entry) {
            QVariantMap map;
            map.insert(QStringLiteral("text"), QString::fromStdString(entry.text));
            map.insert(QStringLiteral("kind"), QString::fromStdString(entry.kind));
            map.insert(QStringLiteral("key"), QString::fromStdString(entry.key));
            return map;
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
        syncAdvancedQuickSector();
        connect(advanced_, &FilterPanelAdvancedViewModel::stateChanged, this, [this]() {
            normalizeAdvancedFilterOverlap();
            sector_.refreshFromState();
            publishFilterStateChange();
        });
        connect(advanced_, &FilterPanelAdvancedViewModel::textFilterApplied, this,
                &FilterPanelViewModel::handleAdvancedTextFilterApplied);
        connect(advanced_, &FilterPanelAdvancedViewModel::applyRequested, this,
                &FilterPanelViewModel::applyRequested);
        connect(&columns_, &ColumnFilterViewModel::stateChanged, this, [this]() {
            if (filterpanel::clearStatusExclusionIfStatusIncludesExcluded(state_)) {
                sector_.refreshFromState();
            }
            advanced_->refreshFromState();
            sector_.refreshFromState();
            synchronizeFilterState(false);
        });
        connect(&columns_, &ColumnFilterViewModel::applyRequested, this,
                &FilterPanelViewModel::applyRequested);
        configureDistinctValueRefresh();
        refreshActiveFilters();
        scheduleColumnValueRefresh();
        refreshQuickSectorOptions();
    }

    void FilterPanelViewModel::loadFilterCatalog() {
        std::ranges::transform(domain::ColumnCatalog::orderedFilterColumnKeys(),
                               std::back_inserter(filterColumnKeys_),
                               [](const auto& key) { return QString::fromStdString(key); });
        std::ranges::transform(domain::ColumnCatalog::weekColumnKeys(),
                               std::back_inserter(weekColumnKeys_), [](const auto key) {
                                   return QString::fromUtf8(key.data(),
                                                            static_cast<qsizetype>(key.size()));
                               });
    }

    void FilterPanelViewModel::configureDistinctValueRefresh() {
        activeFilterRefreshTimer_.setInterval(kActiveFilterRefreshDelayMs);
        activeFilterRefreshTimer_.setSingleShot(true);
        connect(&activeFilterRefreshTimer_, &QTimer::timeout, this, [this]() {
            refreshActiveFilters();
            emit changed();
        });
        connect(&distinctValues_, &FilterPanelDistinctValuesController::columnValueOptionsReady,
                this,
                [this](const std::vector<std::string>& values, const QString& key,
                       const std::uint64_t stateVersion) {
                    setColumnValueOptions(values, key, stateVersion);
                });
        connect(&distinctValues_, &FilterPanelDistinctValuesController::quickSectorOptionsReady,
                this,
                [this](const std::vector<std::string>& values) { sector_.setOptions(values); });
        connect(&sector_, &FilterPanelSectorViewModel::stateChanged, this,
                [this](const bool quickSectorChanged) {
                    if (quickSectorChanged) {
                        advanced_->refreshFromState();
                    }
                    publishFilterStateChange(quickSectorChanged);
                });
    }

    QString FilterPanelViewModel::quickSector() const {
        return state_.quickSector();
    }

    void FilterPanelViewModel::setQuickSector(const QString& value) {
        sector_.setQuickSector(value);
    }

    bool FilterPanelViewModel::excludeScaSesSte() const {
        return sector_.excludeScaSesSte();
    }

    void FilterPanelViewModel::setExcludeScaSesSte(bool value) {
        const bool didChange = sector_.excludeScaSesSte() != value;
        if (!didChange) {
            return;
        }
        sector_.setExcludeScaSesSte(value);
        if (filterpanel::clearStatusExclusionIfStatusIncludesExcluded(state_)) {
            sector_.refreshFromState();
        }
        publishFilterStateChange();
        emit applyRequested();
    }

    QStringList FilterPanelViewModel::filterColumnKeys() const {
        return filterColumnKeys_;
    }

    QStringList FilterPanelViewModel::statusShortcutValues() const {
        QStringList values;
        values.reserve(static_cast<qsizetype>(kStatusShortcutValues.size()));
        std::ranges::transform(
            kStatusShortcutValues, std::back_inserter(values), [](const auto value) {
                return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
            });
        return values;
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
        return columnValueOptions_.version();
    }

    int FilterPanelViewModel::focusColumnRequest() const {
        return focusColumnRequest_;
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
        if (!columnValueOptions_.hasFreshOptions(key, filterStateVersion_)) {
            return {};
        }
        return columnValueOptions_.optionsFor(key);
    }

    QStringList FilterPanelViewModel::columnValuePreviewOptionsFor(const QString& key,
                                                                   const int limit,
                                                                   const bool expanded) const {
        if (!columnValueOptions_.hasFreshOptions(key, filterStateVersion_)) {
            return {};
        }
        return columnValueOptions_.previewOptionsFor(key, limit, expanded);
    }

    bool FilterPanelViewModel::hasMoreColumnValueOptionsFor(const QString& key,
                                                            const int limit) const {
        if (!columnValueOptions_.hasFreshOptions(key, filterStateVersion_)) {
            return false;
        }
        return columnValueOptions_.hasMoreOptionsFor(key, limit);
    }

    bool FilterPanelViewModel::columnValueOptionsLoadingFor(const QString& key) const {
        return columnValueOptions_.loadingFor(key);
    }

    bool FilterPanelViewModel::statusShortcutSelected(const QString& code) const {
        const auto normalizedCode = code.trimmed().toUpper().toStdString();
        if (normalizedCode.empty()) {
            return false;
        }
        const auto statusKey = statusColumnKey();
        const auto advancedExpression = state_.advanced().textFilter(statusKey).toStdString();
        const auto columnFilter = state_.columnFilters().find(statusKey.toStdString());
        const auto tokens = query::parseTextFilterTokens(
            advancedExpression.empty() && columnFilter != state_.columnFilters().end()
                ? columnFilter->second
                : advancedExpression);
        if (tokens.ordered.empty()) {
            return false;
        }
        return std::ranges::any_of(tokens.ordered, [&normalizedCode](const auto& token) {
            return token.filterOperator == query::TextFilterOperator::Equals &&
                   token.value == normalizedCode;
        });
    }

    void FilterPanelViewModel::toggleStatusShortcut(const QString& code) {
        const auto normalizedCode = code.trimmed().toUpper().toStdString();
        if (normalizedCode.empty()) {
            return;
        }
        const auto statusKey = statusColumnKey();
        const auto advancedExpression = state_.advanced().textFilter(statusKey).toStdString();
        const auto columnFilter = state_.columnFilters().find(statusKey.toStdString());
        auto tokens = query::parseTextFilterTokens(
            advancedExpression.empty() && columnFilter != state_.columnFilters().end()
                ? columnFilter->second
                : advancedExpression);
        const auto hasToken = [&tokens, &normalizedCode](const query::TextFilterOperator op) {
            return std::ranges::any_of(tokens.ordered, [&normalizedCode, op](const auto& token) {
                return token.value == normalizedCode && token.filterOperator == op;
            });
        };
        const bool hasIncluded = hasToken(query::TextFilterOperator::Equals);
        const bool hasExcluded = hasToken(query::TextFilterOperator::Different);
        query::TextFilterTokenSet nextTokens;
        for (const auto& token : tokens.ordered) {
            if (token.value == normalizedCode) {
                continue;
            }
            query::addTextFilterValue(nextTokens, token.value, token.filterOperator);
        }
        if (hasIncluded) {
            query::addTextFilterValue(nextTokens, normalizedCode,
                                      query::TextFilterOperator::Different);
        } else if (!hasExcluded) {
            query::addTextFilterValue(nextTokens, normalizedCode,
                                      query::TextFilterOperator::Equals);
        }
        tokens = std::move(nextTokens);

        const auto nextExpression = QString::fromStdString(query::joinTextFilterTokens(tokens));
        const bool advancedChanged = state_.advanced().setTextFilter(statusKey, nextExpression);
        const bool columnChanged = state_.removeColumnFilter(statusKey);
        if (!advancedChanged && !columnChanged) {
            return;
        }
        if (columnChanged) {
            columns_.refreshFromState();
        }
        normalizeAdvancedFilterOverlap();
        advanced_->refreshFromState();
        synchronizeFilterState(false);
        emit applyRequested();
    }

    void FilterPanelViewModel::clearStatusShortcuts() {
        const auto statusKey = statusColumnKey();
        const auto advancedExpression = state_.advanced().textFilter(statusKey).toStdString();
        const auto columnFilter = state_.columnFilters().find(statusKey.toStdString());
        const auto tokens = query::parseTextFilterTokens(
            advancedExpression.empty() && columnFilter != state_.columnFilters().end()
                ? columnFilter->second
                : advancedExpression);
        query::TextFilterTokenSet remainingTokens;
        for (const auto& token : tokens.ordered) {
            const bool isShortcutValue = std::ranges::any_of(
                kStatusShortcutValues, [&token](const auto value) { return token.value == value; });
            if (!isShortcutValue) {
                query::addTextFilterValue(remainingTokens, token.value, token.filterOperator);
            }
        }
        const bool advancedChanged = state_.advanced().setTextFilter(
            statusKey, QString::fromStdString(query::joinTextFilterTokens(remainingTokens)));
        const bool columnChanged = state_.removeColumnFilter(statusKey);
        if (!advancedChanged && !columnChanged) {
            return;
        }
        columns_.refreshFromState();
        normalizeAdvancedFilterOverlap();
        advanced_->refreshFromState();
        synchronizeFilterState(false);
        emit applyRequested();
    }

    void FilterPanelViewModel::requestColumnFocus(const QString& key) {
        const bool didChange = state_.setColumnKey(key);
        if (didChange) {
            publishFilterStateChange();
        }
        ++focusColumnRequest_;
        emit focusColumnRequestChanged();
    }

    bool FilterPanelViewModel::removeActiveFilter(const QVariantMap& entry) {
        const auto action = entry.value(QStringLiteral("kind")).toString().trimmed();
        const auto key = entry.value(QStringLiteral("key")).toString();
        bool didChange = false;
        if (action == "quick_sector") {
            didChange = state_.setQuickSector({});
            if (didChange) {
                sector_.refreshFromState();
            }
        } else if (action == "executor_combined") {
            const bool sectorChanged = state_.setQuickSector({});
            const bool textChanged = state_.advanced().setTextFilter(executorColumnKey(), {});
            didChange = sectorChanged || textChanged;
            if (sectorChanged) {
                sector_.refreshFromState();
            }
        } else if (action == "column") {
            didChange = state_.removeColumnFilter(key);
            if (didChange) {
                columns_.refreshFromState();
            }
        } else if (action == "advanced_text") {
            didChange = state_.advanced().setTextFilter(key, {});
            if (didChange && key.trimmed() == executorColumnKey()) {
                sector_.refreshFromState();
            }
        } else if (action == "advanced_year") {
            didChange = state_.advanced().setYear({});
        } else if (action == "advanced_week") {
            didChange = state_.advanced().setWeek({});
        } else if (action == "advanced_issue_year") {
            didChange = state_.advanced().setIssueYear({});
        } else if (action == "advanced_execution_year") {
            didChange = state_.advanced().setExecutionYear({});
        } else if (action == "advanced_reprogramming") {
            const bool equalsChanged = state_.advanced().setReprogrammingEquals({});
            const bool valuesChanged = state_.advanced().setReprogrammingValues({});
            didChange = equalsChanged || valuesChanged;
        } else if (action == "advanced_issue_week_range") {
            const bool startChanged = state_.advanced().setIssueWeekStart({});
            const bool endChanged = state_.advanced().setIssueWeekEnd({});
            didChange = startChanged || endChanged;
        } else if (action == "advanced_execution_week_range") {
            const bool startChanged = state_.advanced().setExecutionWeekStart({});
            const bool endChanged = state_.advanced().setExecutionWeekEnd({});
            didChange = startChanged || endChanged;
        } else if (action == "advanced_derivation_mode") {
            didChange = state_.advanced().setDerivationMode(QStringLiteral("all"));
        } else if (action == "advanced_only_reprogrammed") {
            didChange = state_.advanced().setOnlyReprogrammed(false);
        }

        if (!didChange) {
            return false;
        }
        advanced_->refreshFromState();
        synchronizeFilterState(action == "quick_sector" || action == "executor_combined");
        emit applyRequested();
        return true;
    }

    void FilterPanelViewModel::setColumnValueOptions(const std::vector<std::string>& options,
                                                     const QString& key,
                                                     const std::uint64_t stateVersion) {
        if (stateVersion != 0 && stateVersion != filterStateVersion_) {
            return;
        }
        const auto normalizedKey = key.trimmed();
        columnValueOptions_.store(options, normalizedKey, filterStateVersion_);
        emit columnValueOptionsChanged();
        emit columnValueOptionsChangedFor(normalizedKey);
    }

    void FilterPanelViewModel::publishFilterStateChange(const bool quickSectorChanged) {
        syncAdvancedQuickSector();
        ++filterStateVersion_;
        distinctValues_.invalidateColumnValueRequests();
        columnValueOptions_.clearLoading();
        columnValueOptions_.touchVersion();
        emit columnValueOptionsReset();
        emit columnValueOptionsChanged();
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
        if (filterpanel::clearStatusExclusionIfStatusIncludesExcluded(state_)) {
            sector_.refreshFromState();
        }
        advanced_->refreshFromState();
        columns_.refreshFromState();
        sector_.refreshFromState();
        synchronizeFilterState(false);
    }

    void FilterPanelViewModel::refreshColumnValueOptions() {
        refreshColumnValueOptionsFor(state_.columnKey());
    }

    void FilterPanelViewModel::refreshColumnValueOptionsFor(const QString& key) {
        const auto normalizedKey = key.trimmed();
        if (columnValueOptions_.loadingFor(normalizedKey)) {
            return;
        }
        if (columnValueOptions_.hasFreshOptions(normalizedKey, filterStateVersion_)) {
            columnValueOptions_.touchVersion();
            emit columnValueOptionsChanged();
            emit columnValueOptionsChangedFor(normalizedKey);
            return;
        }
        columnValueOptions_.markLoading(normalizedKey);
        emit columnValueOptionsChanged();
        emit columnValueOptionsChangedFor(normalizedKey);
        distinctValues_.preloadColumnValueOptionsFor(QStringList{normalizedKey},
                                                     filterStateVersion_);
    }

    void FilterPanelViewModel::preloadAdvancedColumnValueOptions() {
        QStringList keys;
        for (const auto key : domain::ColumnCatalog::advancedFilterKeys()) {
            const auto qKey = QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
            if (columnValueOptions_.loadingFor(qKey) ||
                columnValueOptions_.hasFreshOptions(qKey, filterStateVersion_)) {
                continue;
            }
            keys.push_back(qKey);
        }

        const QString reprogrammingKey = QStringLiteral("num_reprogramacoes");
        if (!columnValueOptions_.loadingFor(reprogrammingKey) &&
            !columnValueOptions_.hasFreshOptions(reprogrammingKey, filterStateVersion_)) {
            keys.push_back(reprogrammingKey);
        }
        if (keys.empty()) {
            return;
        }
        for (const auto& key : keys) {
            columnValueOptions_.markLoading(key);
            emit columnValueOptionsChangedFor(key);
        }
        emit columnValueOptionsChanged();
        distinctValues_.preloadColumnValueOptionsFor(keys, filterStateVersion_);
    }

    void FilterPanelViewModel::refreshQuickSectorOptions() {
        distinctValues_.refreshQuickSectorOptions();
    }

    void FilterPanelViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        if (!state_.applyPreferences(snapshot, weekColumnKeys_)) {
            return;
        }
        sector_.refreshFromState();
        normalizeAdvancedFilterOverlap();
        syncAdvancedQuickSector();
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
        syncAdvancedQuickSector();
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

    void FilterPanelViewModel::syncAdvancedQuickSector() {
        if (advanced_ == nullptr) {
            return;
        }
        advanced_->setQuickSector(state_.quickSector());
    }

    bool FilterPanelViewModel::normalizeAdvancedFilterOverlap() {
        const bool columnsChanged = filterpanel::removeColumnFiltersShadowedByAdvancedText(state_);
        const bool exclusionChanged =
            filterpanel::clearStatusExclusionIfStatusIncludesExcluded(state_);
        if (columnsChanged) {
            columns_.refreshFromState();
        }
        if (exclusionChanged) {
            sector_.refreshFromState();
        }
        return columnsChanged || exclusionChanged;
    }

    void FilterPanelViewModel::handleAdvancedTextFilterApplied(const QString& key,
                                                               const QString& expression) {
        if (key.trimmed() != executorColumnKey() || state_.quickSector().trimmed().isEmpty()) {
            normalizeAdvancedFilterOverlap();
            return;
        }

        bool didChange = false;
        if (expression.trimmed().isEmpty()) {
            didChange = state_.setQuickSector({});
        } else {
            const auto mergedExpression =
                QString::fromStdString(filterpanel::executorFilterWithQuickSector(
                    expression.toStdString(), state_.quickSector().toStdString()));
            const bool textChanged = state_.advanced().setTextFilter(key, mergedExpression);
            const bool sectorChanged = state_.setQuickSector({});
            didChange = textChanged || sectorChanged;
        }

        if (!didChange) {
            return;
        }
        sector_.refreshFromState();
        syncAdvancedQuickSector();
        normalizeAdvancedFilterOverlap();
        advanced_->refreshFromState();
        publishFilterStateChange(true);
    }

} // namespace ssa::presentation
