#include "presentation/FilterPanelViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/TextFilterToken.h"
#include "presentation/FilterPanelCanonicalizer.h"
#include "presentation/FilterPanelStateHelpers.h"

#include <QScopedValueRollback>
#include <QVariantMap>

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ssa::presentation {
    namespace {
        constexpr int kActiveFilterRefreshDelayMs = 120;
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

        bool containsExcludedToken(const std::map<std::string, std::string>& filters) {
            return std::ranges::any_of(filters, [](const auto& filter) {
                const auto tokens = domain::parseTextFilterTokens(filter.second);
                return std::ranges::any_of(tokens.ordered, [](const auto& token) {
                    return token.filterOperator == domain::TextFilterOperator::Different;
                });
            });
        }

    } // namespace

    FilterPanelViewModel::FilterPanelViewModel(
        std::shared_ptr<ports::ISsaBrowsePort> browsePort,
        std::shared_ptr<ports::IExecutadasReportPort> reportPort, QObject* parent)
        : QObject(parent), state_{domain::ColumnCatalog::defaultFilterColumnKey()},
          browsePort_(std::move(browsePort)), columns_(state_, this), sector_(state_, this),
          distinctValues_(browsePort_, state_, this), activeFilterRefreshTimer_(this) {
        if (!reportPort) {
            reportPort = std::dynamic_pointer_cast<ports::IExecutadasReportPort>(browsePort_);
        }
        loadFilterCatalog();
        advanced_ = new FilterPanelAdvancedViewModel(state_.advanced(), state_, weekColumnKeys_,
                                                     std::move(reportPort), this);
        syncAdvancedQuickSector();
        connect(advanced_, &FilterPanelAdvancedViewModel::stateChanged, this, [this]() {
            normalizeAdvancedFilterOverlap();
            sector_.refreshFromState();
            if (!stateReplacementInProgress_) {
                publishFilterStateChange();
            }
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
        refreshQuickSectorOptions();
    }

    void FilterPanelViewModel::loadFilterCatalog() {
        std::ranges::transform(domain::ColumnCatalog::weekColumnKeys(),
                               std::back_inserter(weekColumnKeys_), [](const auto key) {
                                   return QString::fromUtf8(key.data(),
                                                            static_cast<qsizetype>(key.size()));
                               });
    }

    void FilterPanelViewModel::configureDistinctValueRefresh() {
        connect(&distinctValues_, &FilterPanelDistinctValuesController::stateChanged, this,
                &FilterPanelViewModel::backgroundActivityChanged);
        connect(advanced_, &FilterPanelAdvancedViewModel::stateChanged, this,
                &FilterPanelViewModel::backgroundActivityChanged);
        activeFilterRefreshTimer_.setInterval(kActiveFilterRefreshDelayMs);
        activeFilterRefreshTimer_.setSingleShot(true);
        connect(&activeFilterRefreshTimer_, &QTimer::timeout, this, [this]() {
            refreshActiveFilters();
            emit changed();
        });
        connect(&distinctValues_, &FilterPanelDistinctValuesController::columnValueOptionsReady,
                this,
                [this](const std::vector<std::string>& values, const std::size_t maxValueLength,
                       const QString& key, const std::uint64_t stateVersion) {
                    setColumnValueOptions(values, maxValueLength, key, stateVersion);
                });
        connect(
            &distinctValues_, &FilterPanelDistinctValuesController::columnValueOptionsFailed, this,
            [this](const QString& key, const std::uint64_t stateVersion, const QString& message) {
                if (stateVersion != 0 && stateVersion != filterStateVersion_) {
                    return;
                }
                const auto normalizedKey = key.trimmed();
                columnValueOptions_.markFailed(normalizedKey, message);
                emit columnValueOptionsChangedFor(normalizedKey);
            });
        connect(&distinctValues_, &FilterPanelDistinctValuesController::columnValueOptionsCanceled,
                this, [this](const QString& key, const std::uint64_t stateVersion) {
                    if (stateVersion != 0 && stateVersion != filterStateVersion_) {
                        return;
                    }
                    const auto normalizedKey = key.trimmed();
                    columnValueOptions_.clearLoadingFor(normalizedKey);
                    emit columnValueOptionsChangedFor(normalizedKey);
                });
        connect(&distinctValues_, &FilterPanelDistinctValuesController::quickSectorOptionsReady,
                this,
                [this](const std::vector<std::string>& values) { sector_.setOptions(values); });
        connect(&distinctValues_, &FilterPanelDistinctValuesController::quickSectorOptionsFailed,
                &sector_, &FilterPanelSectorViewModel::setOptionsError);
        connect(&sector_, &FilterPanelSectorViewModel::stateChanged, this,
                [this](const bool quickSectorChanged) {
                    if (quickSectorChanged) {
                        advanced_->refreshFromState();
                    }
                    publishFilterStateChange(quickSectorChanged);
                });
    }

    bool FilterPanelViewModel::backgroundWorkRunning() const {
        return distinctValues_.running() || advanced_->backgroundWorkRunning();
    }

    void FilterPanelViewModel::cancelBackgroundWork() {
        distinctValues_.cancel();
        advanced_->cancelBackgroundWork();
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
        if (!state_.setExcludeScaSesSte(value)) {
            return;
        }
        filterpanel::clearStatusExclusionIfStatusIncludesExcluded(state_);
        sector_.refreshFromState();
        publishFilterStateChange();
        emit applyRequested();
    }

    QStringList FilterPanelViewModel::statusShortcutValues() const {
        QStringList values;
        const auto codes = domain::ColumnCatalog::statusShortcutCodes();
        values.reserve(static_cast<qsizetype>(codes.size()));
        std::ranges::transform(codes, std::back_inserter(values), [](const auto value) {
            return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
        });
        return values;
    }

    QString FilterPanelViewModel::excludedStatusCodesText() const {
        QStringList values;
        const auto codes = domain::ColumnCatalog::excludedStatusCodes();
        values.reserve(static_cast<qsizetype>(codes.size()));
        std::ranges::transform(codes, std::back_inserter(values), [](const auto value) {
            return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
        });
        return values.join('/');
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

    bool FilterPanelViewModel::hasExclusionFilter() const {
        if (state_.excludeScaSesSte() || containsExcludedToken(state_.columnFilters())) {
            return true;
        }
        return containsExcludedToken(state_.advancedFilters().textFilters);
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

    int FilterPanelViewModel::columnValueMaxLengthFor(const QString& key) const {
        if (!columnValueOptions_.hasFreshOptions(key, filterStateVersion_)) {
            return 0;
        }
        return static_cast<int>(columnValueOptions_.maxValueLengthFor(key));
    }

    bool FilterPanelViewModel::columnValueOptionsLoadingFor(const QString& key) const {
        return columnValueOptions_.loadingFor(key);
    }

    QString FilterPanelViewModel::columnValueOptionsErrorFor(const QString& key) const {
        return columnValueOptions_.errorFor(key);
    }

    bool FilterPanelViewModel::statusShortcutSelected(const QString& code) const {
        const auto normalizedCode = code.trimmed().toUpper().toStdString();
        if (normalizedCode.empty()) {
            return false;
        }
        const auto statusKey = statusColumnKey();
        const auto advancedExpression = state_.advanced().textFilter(statusKey).toStdString();
        const auto columnFilter = state_.columnFilters().find(statusKey.toStdString());
        const auto tokens = domain::parseTextFilterTokens(
            advancedExpression.empty() && columnFilter != state_.columnFilters().end()
                ? columnFilter->second
                : advancedExpression);
        if (tokens.ordered.empty()) {
            return false;
        }
        return std::ranges::any_of(tokens.ordered, [&normalizedCode](const auto& token) {
            return token.filterOperator == domain::TextFilterOperator::Equals &&
                   token.value == normalizedCode;
        });
    }

    int FilterPanelViewModel::statusShortcutState(const QString& code) const {
        const auto normalizedCode = code.trimmed().toUpper().toStdString();
        if (normalizedCode.empty()) {
            return 0;
        }
        const auto statusKey = statusColumnKey();
        const auto advancedExpression = state_.advanced().textFilter(statusKey).toStdString();
        const auto columnFilter = state_.columnFilters().find(statusKey.toStdString());
        const auto tokens = domain::parseTextFilterTokens(
            advancedExpression.empty() && columnFilter != state_.columnFilters().end()
                ? columnFilter->second
                : advancedExpression);
        const auto hasOperator = [&tokens, &normalizedCode](const domain::TextFilterOperator op) {
            return std::ranges::any_of(tokens.ordered, [&normalizedCode, op](const auto& token) {
                return token.value == normalizedCode && token.filterOperator == op;
            });
        };
        // Included (=) wins over Excluded (!): a single value cannot hold both
        // operators because TextFilterToken dedups by value.
        if (hasOperator(domain::TextFilterOperator::Equals)) {
            return 1;
        }
        if (hasOperator(domain::TextFilterOperator::Different)) {
            return 2;
        }
        return 0;
    }

    void FilterPanelViewModel::toggleStatusShortcut(const QString& code) {
        const auto normalizedCode = code.trimmed().toUpper().toStdString();
        if (normalizedCode.empty()) {
            return;
        }
        const auto statusKey = statusColumnKey();
        const auto advancedExpression = state_.advanced().textFilter(statusKey).toStdString();
        const auto columnFilter = state_.columnFilters().find(statusKey.toStdString());
        auto tokens = domain::parseTextFilterTokens(
            advancedExpression.empty() && columnFilter != state_.columnFilters().end()
                ? columnFilter->second
                : advancedExpression);
        const auto hasToken = [&tokens, &normalizedCode](const domain::TextFilterOperator op) {
            return std::ranges::any_of(tokens.ordered, [&normalizedCode, op](const auto& token) {
                return token.value == normalizedCode && token.filterOperator == op;
            });
        };
        const bool hasIncluded = hasToken(domain::TextFilterOperator::Equals);
        const bool hasExcluded = hasToken(domain::TextFilterOperator::Different);
        domain::TextFilterTokenSet nextTokens;
        for (const auto& token : tokens.ordered) {
            if (token.value == normalizedCode) {
                continue;
            }
            domain::addTextFilterValue(nextTokens, token.value, token.filterOperator);
        }
        if (hasIncluded) {
            domain::addTextFilterValue(nextTokens, normalizedCode,
                                       domain::TextFilterOperator::Different);
        } else if (!hasExcluded) {
            domain::addTextFilterValue(nextTokens, normalizedCode,
                                       domain::TextFilterOperator::Equals);
        }
        tokens = std::move(nextTokens);

        const auto nextExpression = QString::fromStdString(domain::joinTextFilterTokens(tokens));
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
        const auto tokens = domain::parseTextFilterTokens(
            advancedExpression.empty() && columnFilter != state_.columnFilters().end()
                ? columnFilter->second
                : advancedExpression);
        domain::TextFilterTokenSet remainingTokens;
        for (const auto& token : tokens.ordered) {
            const auto shortcutCodes = domain::ColumnCatalog::statusShortcutCodes();
            const bool isShortcutValue = std::ranges::any_of(
                shortcutCodes, [&token](const auto value) { return token.value == value; });
            if (!isShortcutValue) {
                domain::addTextFilterValue(remainingTokens, token.value, token.filterOperator);
            }
        }
        const bool advancedChanged = state_.advanced().setTextFilter(
            statusKey, QString::fromStdString(domain::joinTextFilterTokens(remainingTokens)));
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
            didChange = state_.advanced().setReprogrammingValues({});
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
                                                     const std::size_t maxValueLength,
                                                     const QString& key,
                                                     const std::uint64_t stateVersion) {
        if (stateVersion != 0 && stateVersion != filterStateVersion_) {
            return;
        }
        const auto normalizedKey = key.trimmed();
        columnValueOptions_.store(options, normalizedKey, filterStateVersion_, maxValueLength);
        emit columnValueOptionsChangedFor(normalizedKey);
    }

    void FilterPanelViewModel::publishFilterStateChange(const bool quickSectorChanged) {
        syncAdvancedQuickSector();
        invalidateColumnValueOptions();
        scheduleActiveFilterRefresh();
        if (!quickSectorChanged) {
            refreshQuickSectorOptions();
        }
    }

    void FilterPanelViewModel::invalidateColumnValueOptions() {
        ++filterStateVersion_;
        distinctValues_.invalidateColumnValueRequests();
        columnValueOptions_.clearLoading();
        emit columnValueOptionsReset();
    }

    void FilterPanelViewModel::setColumnFilters(std::map<std::string, std::string> filters) {
        if (!state_.setColumnFilters(std::move(filters))) {
            return;
        }
        {
            QScopedValueRollback replacement(stateReplacementInProgress_, true);
            invalidateColumnValueOptions();
            if (filterpanel::clearStatusExclusionIfStatusIncludesExcluded(state_)) {
                sector_.refreshFromState();
            }
            advanced_->refreshFromState();
            columns_.refreshFromState();
            sector_.refreshFromState();
        }
        synchronizeFilterState(false);
    }

    void FilterPanelViewModel::refreshColumnValueOptionsFor(const QString& key) {
        const auto normalizedKey = key.trimmed();
        if (columnValueOptions_.loadingFor(normalizedKey)) {
            return;
        }
        if (columnValueOptions_.hasFreshOptions(normalizedKey, filterStateVersion_)) {
            emit columnValueOptionsChangedFor(normalizedKey);
            return;
        }
        columnValueOptions_.markLoading(normalizedKey);
        emit columnValueOptionsChangedFor(normalizedKey);
        distinctValues_.refreshColumnValueOptionsFor(normalizedKey, filterStateVersion_);
    }

    void FilterPanelViewModel::invalidateDataSourceOptions() {
        invalidateColumnValueOptions();
        refreshQuickSectorOptions();
    }

    void FilterPanelViewModel::retryQuickSectorOptions() {
        refreshQuickSectorOptions();
    }

    void FilterPanelViewModel::refreshQuickSectorOptions() {
        distinctValues_.refreshQuickSectorOptions();
    }

    void FilterPanelViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        if (!state_.applyPreferences(snapshot, weekColumnKeys_)) {
            return;
        }
        {
            QScopedValueRollback replacement(stateReplacementInProgress_, true);
            invalidateColumnValueOptions();
            sector_.refreshFromState();
            normalizeAdvancedFilterOverlap();
            syncAdvancedQuickSector();
            advanced_->refreshFromState();
            columns_.refreshFromState();
        }
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
        {
            QScopedValueRollback replacement(stateReplacementInProgress_, true);
            invalidateColumnValueOptions();
            sector_.refreshFromState();
            syncAdvancedQuickSector();
            advanced_->refreshFromState();
            columns_.refreshFromState();
        }
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
        if (refreshSectorOptions) {
            refreshQuickSectorOptions();
        }
        emit changed();
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
