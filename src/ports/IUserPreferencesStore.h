#pragma once

#include <map>
#include <string>
#include <vector>

namespace ssa::ports {

    struct UserPreferencesSnapshot {
        int schemaVersion{1};
        std::vector<std::string> visibleColumns;
        std::map<std::string, int> columnWidths;
        int pageSize{100};
        std::string theme{"system"};
        std::string quickSector;
        bool excludeClosedStatuses{true};
        std::map<std::string, std::string> columnFilters;
    };

    class IUserPreferencesStore {
      public:
        virtual ~IUserPreferencesStore() = default;

        [[nodiscard]] virtual UserPreferencesSnapshot load() const = 0;
        virtual void save(const UserPreferencesSnapshot& snapshot) const = 0;
    };

} // namespace ssa::ports
