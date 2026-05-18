#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/FilterPanelAdvancedState.h"

#include <QString>
#include <QStringList>

#include <map>
#include <string>

namespace ssa::presentation::filterpanel {

    class FilterPanelState final {
      public:
        explicit FilterPanelState(std::string_view defaultFilterColumnKey =
                                      domain::ColumnCatalog::defaultFilterColumnKey());

        [[nodiscard]] const QString& quickSector() const;
        bool setQuickSector(QString value);
        [[nodiscard]] bool excludeScaSesSte() const;
        bool setExcludeScaSesSte(bool value);

        [[nodiscard]] const QString& columnKey() const;
        bool setColumnKey(const QString& value);
        [[nodiscard]] const QString& columnValue() const;
        bool setColumnValue(QString value);

        [[nodiscard]] FilterPanelAdvancedState& advanced();
        [[nodiscard]] const FilterPanelAdvancedState& advanced() const;

        [[nodiscard]] const std::map<std::string, std::string>& columnFilters() const;
        bool setColumnFilters(std::map<std::string, std::string> filters);
        bool addColumnFilter(const QString& key, const QString& value);
        bool removeColumnFilter(const QString& key);

        bool applyPreferences(const ports::UserPreferencesSnapshot& snapshot,
                              const QStringList& weekColumnKeys);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;

        [[nodiscard]] domain::AdvancedFilterSpec advancedFilters() const;
        [[nodiscard]] bool hasFilterForColumn(const QString& key) const;
        void clear();

      private:
        QString quickSector_;
        bool excludeScaSesSte_{domain::kDefaultExcludeScaSesSte};
        QString columnKey_;
        QString columnValue_;
        FilterPanelAdvancedState advanced_;
        std::map<std::string, std::string> columnFilters_;
    };

} // namespace ssa::presentation::filterpanel
