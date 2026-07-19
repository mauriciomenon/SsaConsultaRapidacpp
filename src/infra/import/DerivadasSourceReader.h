#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::infra::sqlite {
    class SqliteDerivadasPort;
}

namespace ssa::infra::importing {

    class DerivadasImportTestAccess;

    enum class DerivadasSourceStatus { Succeeded, Rejected, Canceled, Failed };

    struct DerivationEdge {
        std::string parent;
        std::string child;
    };

    struct DerivadasSourceResult {
        DerivadasSourceStatus status = DerivadasSourceStatus::Failed;
        std::vector<DerivationEdge> edges;
        std::size_t inputRows = 0;
        std::string message;
        std::string diagnostic;

        [[nodiscard]] bool ok() const {
            return status == DerivadasSourceStatus::Succeeded;
        }
    };

    enum class DerivadasMergeStatus { Succeeded, Rejected, Canceled };

    struct DerivadasMergeResult {
        DerivadasMergeStatus status = DerivadasMergeStatus::Rejected;
        std::string message;
    };

    class DerivadasEdgeMerger final {
      public:
        [[nodiscard]] DerivadasMergeResult add(std::span<const DerivationEdge> edges,
                                               const std::stop_token& stopToken = {});
        [[nodiscard]] const std::map<std::string, std::string>& parentByChild() const;
        [[nodiscard]] std::size_t duplicates() const;

      private:
        friend class ssa::infra::sqlite::SqliteDerivadasPort;

        [[nodiscard]] DerivadasMergeResult add(std::span<const DerivationEdge> edges,
                                               const std::stop_token& stopToken,
                                               const std::function<void()>& afterFirstEdgeMerged);

        std::map<std::string, std::string> parentByChild_;
        std::set<std::pair<std::string, std::string>> uniqueEdges_;
        std::size_t duplicates_ = 0;
    };

    class DerivadasSourceReader final {
      public:
        [[nodiscard]] static DerivadasSourceResult read(const std::filesystem::path& source,
                                                        const std::stop_token& stopToken = {});

      private:
        friend class ssa::infra::sqlite::SqliteDerivadasPort;
        friend class DerivadasImportTestAccess;

        [[nodiscard]] static DerivadasSourceResult
        read(const std::filesystem::path& source, const std::stop_token& stopToken,
             const std::function<void()>& afterFirstParsingChunk);
    };

} // namespace ssa::infra::importing
