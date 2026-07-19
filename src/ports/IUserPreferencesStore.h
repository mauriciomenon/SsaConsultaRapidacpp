#pragma once

#include "domain/SsaTypes.h"
#include "ports/UserPreferenceDefaults.h"

#include <cstddef>
#include <map>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::ports {

    inline constexpr int kCurrentUserPreferencesSchemaVersion = 14;
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

        bool operator==(const FilterPreferencesSnapshot&) const = default;
    };

    struct SavedFilterSnapshot {
        std::string name;
        FilterPreferencesSnapshot filters;
    };

    struct SamRefreshPreferencesSnapshot {
        std::string scrapReportRoot;
        std::string caFile;
        std::string baseUrl{"https://apps.itaipu.gov.br/SAM_SMA_API/rest/SSA_API"};
        std::string executorSectors;
        std::string scope{"consulta"};
        int intervalMinutes{30'000};
        bool enabled{false};
        bool autoRefreshEnabled{false};

        bool operator==(const SamRefreshPreferencesSnapshot&) const = default;
    };

    struct ImportExecutionPreferencesSnapshot {
        static constexpr int kDefaultRowsPerChunk = 1'000;
        static constexpr int kMaxRowsPerChunk = 1'000;
        static constexpr int kDefaultSqliteBusyWaitMs = 3'000;
        static constexpr int kMaxSqliteBusyWaitMs = 3'000;
        static constexpr int kSqliteBusyRetryGranularityMs = 5;

        int rowsPerChunk{kDefaultRowsPerChunk};
        int sqliteBusyWaitMs{kDefaultSqliteBusyWaitMs};

        bool operator==(const ImportExecutionPreferencesSnapshot&) const = default;
    };

    struct UserPreferencesSnapshot {
        std::vector<std::string> visibleColumns;
        std::map<std::string, int> columnWidths;
        std::vector<SavedFilterSnapshot> savedFilters;
        std::string theme{"ssa-dark"};
        std::string density{"compact"};
        std::string sortColumnKey{"numero_ssa"};
        FilterPreferencesSnapshot filters;
        SamRefreshPreferencesSnapshot samRefresh;
        ImportExecutionPreferencesSnapshot importExecution;
        int schemaVersion{kCurrentUserPreferencesSchemaVersion};
        int pageSize{domain::kDefaultPageSize};
        int detailsPanelWidth{ports::kDefaultDetailsPanelWidth};
        bool detailsVisible{true};
        bool sortAscending{false};
    };

    class IUserPreferencesStore {
      public:
        virtual ~IUserPreferencesStore() = default;

        [[nodiscard]] virtual UserPreferencesSnapshot
        load(std::stop_token stopToken = {}) const = 0;
        virtual void save(const UserPreferencesSnapshot& snapshot,
                          std::stop_token stopToken = {}) const = 0;
    };

} // namespace ssa::ports
