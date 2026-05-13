#pragma once

#include "domain/SsaTypes.h"
#include "ports/UserPreferenceDefaults.h"

#include <map>
#include <string>
#include <vector>

namespace ssa::ports {

    struct UserPreferencesSnapshot {
        int schemaVersion{1};
        std::vector<std::string> visibleColumns;
        std::map<std::string, int> columnWidths;
        int pageSize{domain::kDefaultPageSize};
        std::string theme{"system"};
        std::string density{"normal"};
        bool detailsVisible{true};
        int detailsPanelWidth{ports::kDefaultDetailsPanelWidth};
        std::string sortColumnKey{"numero_ssa"};
        bool sortAscending{false};
        std::string quickSector;
        bool excludeScaSesSte{true};
        std::map<std::string, std::string> columnFilters;
        std::string advancedWeekColumnKey{"semana_programada"};
        std::string advancedYear;
        std::string advancedWeek;
        std::string derivationMode{"all"};
        bool onlyReprogrammed{false};
    };

    [[nodiscard]] inline std::string normalizedDerivationMode(std::string value) {
        if (value == "root" || value == "derived") {
            return value;
        }
        return "all";
    }

    class IUserPreferencesStore {
      public:
        virtual ~IUserPreferencesStore() = default;

        [[nodiscard]] virtual UserPreferencesSnapshot load() const = 0;
        virtual void save(const UserPreferencesSnapshot& snapshot) const = 0;
    };

} // namespace ssa::ports
