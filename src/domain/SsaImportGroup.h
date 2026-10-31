#pragma once

#include "domain/SsaImportPolicy.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

namespace ssa::domain {

    // Every row of one SSA seen in an import run, folded so the result never depends on the
    // order the rows arrive in (Python _consolidate_upsert_rows). Rows are ranked by
    // precedence, then content, then source name; each field takes the value of the
    // highest-ranked row that has it (the newest overwrites, an older row fills a gap). When
    // any row is terminal (STE/SCA) the open rows are dropped and only the terminal rows of
    // the highest precedence count.
    class SsaImportGroup final {
      public:
        using Values = SsaImportPolicy::Values;

        // `source` is returned by winnerSource(); `sourceName` (unique per source: the file
        // path, never its basename) breaks a tie between equal rows of two sources without
        // depending on the read order.
        void add(const Values& row, std::size_t source, const std::string& sourceName);

        [[nodiscard]] Values values() const;
        // Source of the highest-ranked row that decided the result.
        [[nodiscard]] std::size_t winnerSource() const;
        [[nodiscard]] std::size_t rows() const noexcept;
        // Precedence of the rows that decide the result; empty when the group has none.
        [[nodiscard]] const std::string& leadingPrecedence() const noexcept;
        // Only overlapping, differing business fields can produce a repeat diagnostic.
        [[nodiscard]] bool hasRepeatDifference(const Values& row) const;
        // Rows of equal precedence disagreed and were resolved by content.
        [[nodiscard]] bool resolvedByContent() const noexcept;
        // Dating fields of the newest open row and of the oldest terminal row; empty when
        // the group has none.
        [[nodiscard]] const Values& newestOpen() const noexcept;
        [[nodiscard]] const Values& oldestTerminal() const noexcept;

      private:
        struct Rank {
            std::string precedence;
            SsaImportPolicy::ContentKey content;
            std::string sourceName;
            std::size_t source = 0;

            [[nodiscard]] bool operator<(const Rank& other) const;
        };
        struct Field {
            std::shared_ptr<const Rank> rank;
            std::string value;
            std::optional<std::string> original;
        };
        struct Fold {
            std::unordered_map<std::string, Field> fields;
            std::shared_ptr<const Rank> top;
            bool resolvedByContent = false;

            void add(const Values& row, const std::shared_ptr<const Rank>& rank);
        };

        static void keepExtreme(std::shared_ptr<const Rank>& kept, Values& dating,
                                const std::shared_ptr<const Rank>& rank, const Values& row,
                                bool newest);

        Fold open_;
        Fold terminal_;
        std::shared_ptr<const Rank> newestOpenRank_;
        std::shared_ptr<const Rank> oldestTerminalRank_;
        Values newestOpen_;
        Values oldestTerminal_;
        std::size_t rows_ = 0;
    };

} // namespace ssa::domain
