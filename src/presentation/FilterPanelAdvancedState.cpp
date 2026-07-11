#include "presentation/FilterPanelAdvancedState.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaDerivationMode.h"
#include "presentation/FilterPanelStateHelpers.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ssa::presentation::filterpanel {
    namespace {

        bool setTrimmed(QString& target, QString value) {
            value = value.trimmed();
            if (target == value) {
                return false;
            }
            target = std::move(value);
            return true;
        }

        template <typename Parser>
        bool setValidated(QString& target, QString value, Parser parser) {
            value = value.trimmed();
            if (!value.isEmpty() && !parser(value.toStdString()).has_value()) {
                return false;
            }
            return setTrimmed(target, std::move(value));
        }

        template <typename Parser>
        QString validatedPreference(const std::string& stored, Parser parser) {
            auto value = QString::fromStdString(stored).trimmed();
            return value.isEmpty() || parser(value.toStdString()).has_value() ? value : QString{};
        }

        std::vector<int> parsePositiveIntList(const QString& value) {
            std::vector<int> result;
            for (const auto& part : value.split(QStringLiteral(","), Qt::SkipEmptyParts)) {
                const auto parsed = parsePositiveInt(part);
                if (parsed.has_value() && std::ranges::find(result, *parsed) == result.end()) {
                    result.push_back(*parsed);
                }
            }
            std::ranges::sort(result);
            return result;
        }

    } // namespace

    FilterPanelAdvancedState::FilterPanelAdvancedState() {
        input_.derivationMode =
            QString::fromStdString(derivationModeToString(domain::DerivationFilterMode::All));
    }

    QString FilterPanelAdvancedState::textFilter(const QString& key) const {
        const auto filterEntry = input_.textFilters.find(key.trimmed().toStdString());
        return filterEntry == input_.textFilters.end()
                   ? QString{}
                   : QString::fromStdString(filterEntry->second);
    }

    bool FilterPanelAdvancedState::setTextFilter(const QString& key, QString value) {
        const auto normalizedKey = key.trimmed().toStdString();
        value = value.trimmed();
        const auto normalizedValue = value.toStdString();
        const auto filterEntry = input_.textFilters.find(normalizedKey);
        if (normalizedValue.empty()) {
            if (filterEntry == input_.textFilters.end()) {
                return false;
            }
            input_.textFilters.erase(filterEntry);
            return true;
        }
        if (!domain::ColumnCatalog::contains(normalizedKey)) {
            return false;
        }
        if (filterEntry != input_.textFilters.end() && filterEntry->second == normalizedValue) {
            return false;
        }
        input_.textFilters[normalizedKey] = normalizedValue;
        return true;
    }

    const std::map<std::string, std::string>& FilterPanelAdvancedState::textFilters() const {
        return input_.textFilters;
    }

    bool FilterPanelAdvancedState::removeTextFilter(const QString& key) {
        return setTextFilter(key, {});
    }

    const QString& FilterPanelAdvancedState::weekColumnKey() const {
        return input_.genericWeek.columnKey;
    }

    bool FilterPanelAdvancedState::setWeekColumnKey(const QString& value,
                                                    const QStringList& validKeys) {
        const auto normalized = value.trimmed();
        if (!validKeys.contains(normalized) || input_.genericWeek.columnKey == normalized) {
            return false;
        }
        input_.genericWeek.columnKey = normalized;
        return true;
    }

    const QString& FilterPanelAdvancedState::year() const {
        return input_.genericWeek.year;
    }

    bool FilterPanelAdvancedState::setYear(QString value) {
        return setValidated(input_.genericWeek.year, std::move(value), domain::parseFilterYear);
    }

    const QString& FilterPanelAdvancedState::week() const {
        return input_.genericWeek.week;
    }

    bool FilterPanelAdvancedState::setWeek(QString value) {
        return setValidated(input_.genericWeek.week, std::move(value), domain::parseIsoWeek);
    }

    const QString& FilterPanelAdvancedState::issueYear() const {
        return input_.years.issue;
    }

    bool FilterPanelAdvancedState::setIssueYear(QString value) {
        return setValidated(input_.years.issue, std::move(value), domain::parseFilterYear);
    }

    const QString& FilterPanelAdvancedState::executionYear() const {
        return input_.years.execution;
    }

    bool FilterPanelAdvancedState::setExecutionYear(QString value) {
        return setValidated(input_.years.execution, std::move(value), domain::parseFilterYear);
    }

    const QString& FilterPanelAdvancedState::reprogrammingEquals() const {
        return input_.reprogramming.equals;
    }

    bool FilterPanelAdvancedState::setReprogrammingEquals(QString value) {
        return setTrimmed(input_.reprogramming.equals, std::move(value));
    }

    const QString& FilterPanelAdvancedState::reprogrammingMode() const {
        return input_.reprogramming.mode;
    }

    bool FilterPanelAdvancedState::setReprogrammingMode(const QString& value) {
        const auto normalized = QString::fromStdString(
            domain::normalizedNumericComparisonMode(value.trimmed().toStdString()));
        if (input_.reprogramming.mode == normalized) {
            return false;
        }
        input_.reprogramming.mode = normalized;
        return true;
    }

    const QString& FilterPanelAdvancedState::reprogrammingValues() const {
        return input_.reprogramming.values;
    }

    bool FilterPanelAdvancedState::setReprogrammingValues(QString value) {
        return setTrimmed(input_.reprogramming.values, std::move(value));
    }

    const QString& FilterPanelAdvancedState::issueWeekStart() const {
        return input_.weekRanges.issueStart;
    }

    bool FilterPanelAdvancedState::setIssueWeekStart(QString value) {
        return setValidated(input_.weekRanges.issueStart, std::move(value),
                            domain::parseIsoYearWeek);
    }

    const QString& FilterPanelAdvancedState::issueWeekEnd() const {
        return input_.weekRanges.issueEnd;
    }

    bool FilterPanelAdvancedState::setIssueWeekEnd(QString value) {
        return setValidated(input_.weekRanges.issueEnd, std::move(value), domain::parseIsoYearWeek);
    }

    const QString& FilterPanelAdvancedState::executionWeekStart() const {
        return input_.weekRanges.executionStart;
    }

    bool FilterPanelAdvancedState::setExecutionWeekStart(QString value) {
        return setValidated(input_.weekRanges.executionStart, std::move(value),
                            domain::parseIsoYearWeek);
    }

    const QString& FilterPanelAdvancedState::executionWeekEnd() const {
        return input_.weekRanges.executionEnd;
    }

    bool FilterPanelAdvancedState::setExecutionWeekEnd(QString value) {
        return setValidated(input_.weekRanges.executionEnd, std::move(value),
                            domain::parseIsoYearWeek);
    }

    const QString& FilterPanelAdvancedState::derivationMode() const {
        return input_.derivationMode;
    }

    bool FilterPanelAdvancedState::setDerivationMode(const QString& value) {
        const auto normalized =
            QString::fromStdString(domain::normalizedDerivationMode(value.trimmed().toStdString()));
        if (input_.derivationMode == normalized) {
            return false;
        }
        input_.derivationMode = normalized;
        return true;
    }

    bool FilterPanelAdvancedState::onlyReprogrammed() const {
        return input_.reprogramming.only;
    }

    bool FilterPanelAdvancedState::setOnlyReprogrammed(const bool value) {
        if (input_.reprogramming.only == value) {
            return false;
        }
        input_.reprogramming.only = value;
        return true;
    }

    FilterPanelAdvancedState::Input
    FilterPanelAdvancedState::inputFromPreferences(const ports::UserPreferencesSnapshot& snapshot,
                                                   const QStringList& weekColumnKeys) const {
        Input input;
        input.textFilters = snapshot.filters.advancedTextFilters;
        const QString requestedWeekColumn =
            QString::fromStdString(snapshot.filters.advancedWeekColumnKey).trimmed();
        input.genericWeek.columnKey = weekColumnKeys.contains(requestedWeekColumn)
                                          ? requestedWeekColumn
                                          : input_.genericWeek.columnKey;
        input.genericWeek.year =
            validatedPreference(snapshot.filters.advancedYear, domain::parseFilterYear);
        input.genericWeek.week =
            validatedPreference(snapshot.filters.advancedWeek, domain::parseIsoWeek);
        input.years.issue =
            validatedPreference(snapshot.filters.issueYear, domain::parseFilterYear);
        input.years.execution =
            validatedPreference(snapshot.filters.executionYear, domain::parseFilterYear);
        input.reprogramming.equals =
            QString::fromStdString(snapshot.filters.reprogrammingEquals).trimmed();
        input.reprogramming.mode = QString::fromStdString(
            domain::normalizedNumericComparisonMode(snapshot.filters.reprogrammingMode));
        input.reprogramming.values =
            QString::fromStdString(snapshot.filters.reprogrammingValues).trimmed();
        input.weekRanges.issueStart =
            validatedPreference(snapshot.filters.issueWeekStart, domain::parseIsoYearWeek);
        input.weekRanges.issueEnd =
            validatedPreference(snapshot.filters.issueWeekEnd, domain::parseIsoYearWeek);
        input.weekRanges.executionStart =
            validatedPreference(snapshot.filters.executionWeekStart, domain::parseIsoYearWeek);
        input.weekRanges.executionEnd =
            validatedPreference(snapshot.filters.executionWeekEnd, domain::parseIsoYearWeek);
        input.derivationMode = QString::fromStdString(
            domain::normalizedDerivationMode(snapshot.filters.derivationMode));
        input.reprogramming.only = snapshot.filters.onlyReprogrammed;
        return input;
    }

    void FilterPanelAdvancedState::writeInputPreferences(const Input& input,
                                                         ports::UserPreferencesSnapshot& snapshot) {
        snapshot.filters.advancedTextFilters = input.textFilters;
        snapshot.filters.advancedWeekColumnKey =
            input.genericWeek.columnKey.trimmed().toStdString();
        snapshot.filters.advancedYear = input.genericWeek.year.trimmed().toStdString();
        snapshot.filters.advancedWeek = input.genericWeek.week.trimmed().toStdString();
        snapshot.filters.issueYear = input.years.issue.trimmed().toStdString();
        snapshot.filters.executionYear = input.years.execution.trimmed().toStdString();
        snapshot.filters.reprogrammingEquals = input.reprogramming.equals.trimmed().toStdString();
        snapshot.filters.reprogrammingMode = domain::normalizedNumericComparisonMode(
            input.reprogramming.mode.trimmed().toStdString());
        snapshot.filters.reprogrammingValues = input.reprogramming.values.trimmed().toStdString();
        snapshot.filters.issueWeekStart = input.weekRanges.issueStart.trimmed().toStdString();
        snapshot.filters.issueWeekEnd = input.weekRanges.issueEnd.trimmed().toStdString();
        snapshot.filters.executionWeekStart =
            input.weekRanges.executionStart.trimmed().toStdString();
        snapshot.filters.executionWeekEnd = input.weekRanges.executionEnd.trimmed().toStdString();
        snapshot.filters.derivationMode = input.derivationMode.trimmed().toStdString();
        snapshot.filters.onlyReprogrammed = input.reprogramming.only;
    }

    bool FilterPanelAdvancedState::applyPreferences(const ports::UserPreferencesSnapshot& snapshot,
                                                    const QStringList& weekColumnKeys) {
        const auto next = inputFromPreferences(snapshot, weekColumnKeys);
        if (input_ == next) {
            return false;
        }
        input_ = next;
        return true;
    }

    void
    FilterPanelAdvancedState::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        writeInputPreferences(input_, snapshot);
    }

    domain::AdvancedFilterSpec FilterPanelAdvancedState::filters() const {
        domain::AdvancedFilterSpec filters;
        filters.weekColumnKey = input_.genericWeek.columnKey.trimmed().toStdString();
        filters.textFilters = input_.textFilters;
        filters.year = domain::parseFilterYear(input_.genericWeek.year.toStdString());
        filters.week = domain::parseIsoWeek(input_.genericWeek.week.toStdString());
        filters.issueYear = domain::parseFilterYear(input_.years.issue.toStdString());
        filters.executionYear = domain::parseFilterYear(input_.years.execution.toStdString());
        filters.reprogrammingEquals = parsePositiveInt(input_.reprogramming.equals);
        filters.reprogrammingValues = parsePositiveIntList(input_.reprogramming.values);
        filters.reprogrammingComparison =
            domain::numericComparisonModeFromString(input_.reprogramming.mode.toStdString());
        filters.issueWeekStart =
            domain::parseIsoYearWeek(input_.weekRanges.issueStart.toStdString());
        filters.issueWeekEnd = domain::parseIsoYearWeek(input_.weekRanges.issueEnd.toStdString());
        filters.executionWeekStart =
            domain::parseIsoYearWeek(input_.weekRanges.executionStart.toStdString());
        filters.executionWeekEnd =
            domain::parseIsoYearWeek(input_.weekRanges.executionEnd.toStdString());
        filters.derivationMode = derivationModeFromString(input_.derivationMode);
        filters.onlyReprogrammed = input_.reprogramming.only;
        return filters;
    }

    bool FilterPanelAdvancedState::hasFilterForColumn(const QString& key) const {
        const auto normalizedKey = key.trimmed().toStdString();
        if (input_.textFilters.contains(normalizedKey)) {
            return true;
        }
        const auto normalizedWeekColumn = input_.genericWeek.columnKey.trimmed().toStdString();
        if (normalizedKey == normalizedWeekColumn &&
            (domain::parseFilterYear(input_.genericWeek.year.toStdString()).has_value() ||
             domain::parseIsoWeek(input_.genericWeek.week.toStdString()).has_value())) {
            return true;
        }
        if (normalizedKey == domain::ColumnCatalog::issueWeekColumnKey() &&
            (domain::parseFilterYear(input_.years.issue.toStdString()).has_value() ||
             domain::parseIsoYearWeek(input_.weekRanges.issueStart.toStdString()).has_value() ||
             domain::parseIsoYearWeek(input_.weekRanges.issueEnd.toStdString()).has_value())) {
            return true;
        }
        if (normalizedKey == domain::ColumnCatalog::executionWeekColumnKey() &&
            (domain::parseFilterYear(input_.years.execution.toStdString()).has_value() ||
             domain::parseIsoYearWeek(input_.weekRanges.executionStart.toStdString()).has_value() ||
             domain::parseIsoYearWeek(input_.weekRanges.executionEnd.toStdString()).has_value())) {
            return true;
        }
        if (normalizedKey == domain::ColumnCatalog::derivationColumnKey() &&
            input_.derivationMode !=
                QString::fromStdString(derivationModeToString(domain::DerivationFilterMode::All))) {
            return true;
        }
        return domain::ColumnCatalog::isReprogrammingColumn(normalizedKey) &&
               (input_.reprogramming.only ||
                parsePositiveInt(input_.reprogramming.equals).has_value());
    }

    void FilterPanelAdvancedState::clear() {
        input_.textFilters.clear();
        input_.genericWeek.columnKey = QString::fromStdString(
            std::string{domain::ColumnCatalog::defaultAdvancedWeekColumnKey()});
        input_.genericWeek.year.clear();
        input_.genericWeek.week.clear();
        input_.years.issue.clear();
        input_.years.execution.clear();
        input_.reprogramming.equals.clear();
        input_.reprogramming.mode = QStringLiteral("gte");
        input_.reprogramming.values.clear();
        input_.weekRanges.issueStart.clear();
        input_.weekRanges.issueEnd.clear();
        input_.weekRanges.executionStart.clear();
        input_.weekRanges.executionEnd.clear();
        input_.derivationMode =
            QString::fromStdString(derivationModeToString(domain::DerivationFilterMode::All));
        input_.reprogramming.only = false;
    }

} // namespace ssa::presentation::filterpanel
