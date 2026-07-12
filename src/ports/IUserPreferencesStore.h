#pragma once

#include "domain/SsaTypes.h"
#include "ports/UserPreferenceDefaults.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace ssa::ports {

    inline constexpr int kCurrentUserPreferencesSchemaVersion = 12;
    inline constexpr std::size_t kMaxSavedFilterCount = 200;
    inline constexpr std::size_t kMaxSavedFilterNameLength = 128;
    inline constexpr std::size_t kMaxFilterExpressionLength = 4096;

    struct FilterPreferencesSnapshot {
        std::string searchText;
        std::string quickSector{"IEE3"};
        std::map<std::string, std::string> columnFilters;
        std::map<std::string, std::string> advancedTextFilters;
        std::string advancedWeekColumnKey{"semana_programada"};
        std::string advancedYear;
        std::string advancedWeek;
        std::string issueYear;
        std::string executionYear;
        std::string reprogrammingMode{"eq"};
        std::string reprogrammingValues;
        std::string issueWeekStart;
        std::string issueWeekEnd;
        std::string executionWeekStart;
        std::string executionWeekEnd;
        std::string derivationMode{"all"};
        bool excludeScaSesSte{domain::kDefaultExcludeScaSesSte};
        bool onlyReprogrammed{false};
    };

    struct SavedFilterSnapshot {
        std::string name;
        FilterPreferencesSnapshot filters;
    };

    struct UserPreferencesSnapshot {
        std::vector<std::string> visibleColumns;
        std::map<std::string, int> columnWidths;
        std::vector<SavedFilterSnapshot> savedFilters;
        std::string theme{"ssa-dark"};
        std::string density{"compact"};
        std::string sortColumnKey{"numero_ssa"};
        FilterPreferencesSnapshot filters;
        int schemaVersion{kCurrentUserPreferencesSchemaVersion};
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
