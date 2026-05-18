#pragma once

#include "domain/SsaTypes.h"
#include "ports/UserPreferenceDefaults.h"

#include <map>
#include <string>
#include <vector>

namespace ssa::ports {

    struct FilterPreferencesSnapshot {
        std::string searchText;
        std::string quickSector;
        std::map<std::string, std::string> columnFilters;
        std::map<std::string, std::string> advancedTextFilters;
        std::string advancedWeekColumnKey{"semana_programada"};
        std::string advancedYear;
        std::string advancedWeek;
        std::string issueYear;
        std::string executionYear;
        std::string reprogrammingEquals;
        std::string issueWeekStart;
        std::string issueWeekEnd;
        std::string executionWeekStart;
        std::string executionWeekEnd;
        std::string derivationMode{"all"};
        bool excludeScaSesSte{true};
        bool onlyReprogrammed{false};
    };

    struct UserPreferencesSnapshot {
        std::vector<std::string> visibleColumns;
        std::map<std::string, int> columnWidths;
        std::string theme{"gruvbox"};
        std::string density{"compact"};
        std::string sortColumnKey{"numero_ssa"};
        FilterPreferencesSnapshot filters;
        int schemaVersion{1};
        int pageSize{domain::kDefaultPageSize};
        int detailsPanelWidth{ports::kDefaultDetailsPanelWidth};
        bool detailsVisible{true};
        bool sortAscending{false};
    };

    class IUserPreferencesStore {
      public:
        virtual ~IUserPreferencesStore() = default;

        [[nodiscard]] virtual UserPreferencesSnapshot load() const = 0;
        virtual void save(const UserPreferencesSnapshot& snapshot) const = 0;
    };

} // namespace ssa::ports
