#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"

#include <QString>
#include <QStringList>

#include <map>
#include <string>

namespace ssa::presentation::filterpanel {

    class FilterPanelAdvancedState final {
      public:
        FilterPanelAdvancedState();

        [[nodiscard]] QString textFilter(const QString& key) const;
        bool setTextFilter(const QString& key, QString value);
        [[nodiscard]] const std::map<std::string, std::string>& textFilters() const;

        [[nodiscard]] const QString& weekColumnKey() const;
        bool setWeekColumnKey(const QString& value, const QStringList& validKeys);
        [[nodiscard]] const QString& year() const;
        bool setYear(QString value);
        [[nodiscard]] const QString& week() const;
        bool setWeek(QString value);
        [[nodiscard]] const QString& issueYear() const;
        bool setIssueYear(QString value);
        [[nodiscard]] const QString& executionYear() const;
        bool setExecutionYear(QString value);
        [[nodiscard]] const QString& reprogrammingEquals() const;
        bool setReprogrammingEquals(QString value);
        [[nodiscard]] const QString& reprogrammingMode() const;
        bool setReprogrammingMode(const QString& value);
        [[nodiscard]] const QString& reprogrammingValues() const;
        bool setReprogrammingValues(QString value);
        [[nodiscard]] const QString& issueWeekStart() const;
        bool setIssueWeekStart(QString value);
        [[nodiscard]] const QString& issueWeekEnd() const;
        bool setIssueWeekEnd(QString value);
        [[nodiscard]] const QString& executionWeekStart() const;
        bool setExecutionWeekStart(QString value);
        [[nodiscard]] const QString& executionWeekEnd() const;
        bool setExecutionWeekEnd(QString value);
        [[nodiscard]] const QString& derivationMode() const;
        bool setDerivationMode(const QString& value);
        [[nodiscard]] bool onlyReprogrammed() const;
        bool setOnlyReprogrammed(bool value);

        bool applyPreferences(const ports::UserPreferencesSnapshot& snapshot,
                              const QStringList& weekColumnKeys);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;
        [[nodiscard]] domain::AdvancedFilterSpec filters() const;
        [[nodiscard]] bool hasFilterForColumn(const QString& key) const;
        void clear();

      private:
        struct Input {
            struct GenericWeek {
                QString columnKey{QString::fromStdString(
                    std::string{domain::ColumnCatalog::defaultAdvancedWeekColumnKey()})};
                QString year;
                QString week;

                bool operator==(const GenericWeek&) const = default;
            };

            struct YearPair {
                QString issue;
                QString execution;

                bool operator==(const YearPair&) const = default;
            };

            struct WeekRangePair {
                QString issueStart;
                QString issueEnd;
                QString executionStart;
                QString executionEnd;

                bool operator==(const WeekRangePair&) const = default;
            };

            struct Reprogramming {
                QString equals;
                QString mode{QStringLiteral("eq")};
                QString values;
                bool only{false};

                bool operator==(const Reprogramming&) const = default;
            };

            std::map<std::string, std::string> textFilters;
            GenericWeek genericWeek;
            YearPair years;
            WeekRangePair weekRanges;
            Reprogramming reprogramming;
            QString derivationMode;

            bool operator==(const Input&) const = default;
        };

        [[nodiscard]] Input inputFromPreferences(const ports::UserPreferencesSnapshot& snapshot,
                                                 const QStringList& weekColumnKeys) const;
        static void writeInputPreferences(const Input& input,
                                          ports::UserPreferencesSnapshot& snapshot);

        Input input_;
    };

} // namespace ssa::presentation::filterpanel
