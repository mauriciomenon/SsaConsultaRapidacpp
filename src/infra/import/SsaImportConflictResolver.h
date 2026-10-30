#pragma once

#include "domain/SsaImportGroup.h"
#include "infra/SsaImportData.h"

#include <cstddef>
#include <map>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace ssa::infra::importing {

    // Normalizes an imported row in place (domain::SsaImportPolicy::normalizeImportedRow);
    // throws ports::OperationError when its SSA number or an SSA reference is invalid.
    void requireValidImportedRow(SsaImportRow& row);

    // Rows of the accepted files of an import run, folded per SSA (domain::SsaImportGroup):
    // the result never depends on the order of the files or of their rows. Every repeat of
    // an SSA in the run counts as a duplicate row.
    class SsaImportRunRows final {
      public:
        // `source` identifies the file in the run and comes back in
        // ResolvedSsaImportRows::sources for the SSAs that file won.
        // Validates every row first: an invalid file throws and adds nothing.
        void add(SsaImportBatch batch, std::size_t source, const std::stop_token& stopToken = {});
        // Consumes the run: the groups are released as their rows are materialized.
        [[nodiscard]] ResolvedSsaImportRows resolve(const std::stop_token& stopToken = {});

      private:
        std::map<std::string, domain::SsaImportGroup> groups_; // ordered: stable write order
        std::vector<std::pair<SsaImportRow, std::size_t>> unkeyed_;
        std::size_t duplicateRows_ = 0;
    };

    class SsaImportConflictResolver final {
      public:
        // Each batch is one source of a single run.
        [[nodiscard]] ResolvedSsaImportRows
        resolveBySsaNumberKeepingUnkeyedRows(const std::vector<SsaImportBatch>& batches) const;
    };

} // namespace ssa::infra::importing
