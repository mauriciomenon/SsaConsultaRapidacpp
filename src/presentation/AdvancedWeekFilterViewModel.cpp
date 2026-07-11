#include "presentation/AdvancedWeekFilterViewModel.h"

#include "domain/SsaTypes.h"

#include <utility>

namespace ssa::presentation {

    AdvancedWeekFilterViewModel::AdvancedWeekFilterViewModel(
        filterpanel::FilterPanelAdvancedState& state, QStringList weekColumnKeys, QObject* parent)
        : QObject(parent), state_(state), weekColumnKeys_(std::move(weekColumnKeys)) {}

    QStringList AdvancedWeekFilterViewModel::weekColumnKeys() const {
        return weekColumnKeys_;
    }

    QString AdvancedWeekFilterViewModel::weekColumnKey() const {
        return state_.weekColumnKey();
    }

    void AdvancedWeekFilterViewModel::setWeekColumnKey(const QString& value) {
        if (!state_.setWeekColumnKey(value, weekColumnKeys_)) {
            return;
        }
        emit changed();
    }

    QString AdvancedWeekFilterViewModel::yearFilter() const {
        return state_.year();
    }

    void AdvancedWeekFilterViewModel::setYearFilter(const QString& value) {
        if (!state_.setYear(value)) {
            return;
        }
        emit changed();
    }

    QString AdvancedWeekFilterViewModel::weekFilter() const {
        return state_.week();
    }

    void AdvancedWeekFilterViewModel::setWeekFilter(const QString& value) {
        if (!state_.setWeek(value)) {
            return;
        }
        emit changed();
    }

    QString AdvancedWeekFilterViewModel::issueYearFilter() const {
        return state_.issueYear();
    }

    void AdvancedWeekFilterViewModel::setIssueYearFilter(const QString& value) {
        if (!state_.setIssueYear(value)) {
            return;
        }
        emit changed();
    }

    QString AdvancedWeekFilterViewModel::executionYearFilter() const {
        return state_.executionYear();
    }

    void AdvancedWeekFilterViewModel::setExecutionYearFilter(const QString& value) {
        if (!state_.setExecutionYear(value)) {
            return;
        }
        emit changed();
    }

    QString AdvancedWeekFilterViewModel::issueWeekStartFilter() const {
        return state_.issueWeekStart();
    }

    void AdvancedWeekFilterViewModel::setIssueWeekStartFilter(const QString& value) {
        if (!state_.setIssueWeekStart(value)) {
            return;
        }
        emit changed();
    }

    QString AdvancedWeekFilterViewModel::issueWeekEndFilter() const {
        return state_.issueWeekEnd();
    }

    void AdvancedWeekFilterViewModel::setIssueWeekEndFilter(const QString& value) {
        if (!state_.setIssueWeekEnd(value)) {
            return;
        }
        emit changed();
    }

    QString AdvancedWeekFilterViewModel::executionWeekStartFilter() const {
        return state_.executionWeekStart();
    }

    void AdvancedWeekFilterViewModel::setExecutionWeekStartFilter(const QString& value) {
        if (!state_.setExecutionWeekStart(value)) {
            return;
        }
        emit changed();
    }

    QString AdvancedWeekFilterViewModel::executionWeekEndFilter() const {
        return state_.executionWeekEnd();
    }

    void AdvancedWeekFilterViewModel::setExecutionWeekEndFilter(const QString& value) {
        if (!state_.setExecutionWeekEnd(value)) {
            return;
        }
        emit changed();
    }

    bool AdvancedWeekFilterViewModel::isYearValid(const QString& value) const {
        const auto trimmed = value.trimmed();
        if (trimmed.isEmpty()) {
            return true;
        }
        return domain::parseFilterYear(trimmed.toStdString()).has_value();
    }

    bool AdvancedWeekFilterViewModel::isWeekValid(const QString& value) const {
        const auto trimmed = value.trimmed();
        if (trimmed.isEmpty()) {
            return true;
        }
        return domain::parseIsoWeek(trimmed.toStdString()).has_value();
    }

    bool AdvancedWeekFilterViewModel::isYearWeekValid(const QString& value) const {
        const auto trimmed = value.trimmed();
        if (trimmed.isEmpty()) {
            return true;
        }
        return domain::parseIsoYearWeek(trimmed.toStdString()).has_value();
    }

    void AdvancedWeekFilterViewModel::refreshFromState() {
        emit changed();
    }

} // namespace ssa::presentation
