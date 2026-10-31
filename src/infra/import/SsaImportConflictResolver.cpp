#include "infra/import/SsaImportConflictResolver.h"

#include "domain/SsaImportPolicy.h"
#include "domain/SsaTypes.h"

#include "ports/OperationError.h"

#include <QDebug>
#include <QString>
#include <system_error>

namespace ssa::infra::importing {

    namespace {

        void throwIfStopped(const std::stop_token& stopToken) {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
        }

    } // namespace

    void requireValidImportedRow(SsaImportRow& row) {
        using Issue = domain::SsaImportPolicy::ImportedRowIssue;
        switch (domain::SsaImportPolicy::normalizeImportedRow(row)) {
        case Issue::InvalidNumber:
            throw ports::OperationError("Falha ao validar identificador SSA importado",
                                        "invalid SSA number in import batch");
        case Issue::InvalidReference:
            throw ports::OperationError("Falha ao validar referencia SSA importada",
                                        "invalid SSA reference in import batch");
        case Issue::InvalidInteger:
            throw ports::OperationError("Falha ao validar valor numerico importado",
                                        "invalid integer value in import batch");
        case Issue::None:
            break;
        }
    }

    void SsaImportRunRows::add(SsaImportBatch batch, const std::size_t source,
                               const std::stop_token& stopToken) {
        const auto ssaNumberKey = std::string{domain::kSsaNumberColumnKey};
        // The full path is unique per file; a batch without one (tests, SAM) uses its source.
        const auto sourceName =
            batch.sourcePath.empty() ? std::to_string(source) : batch.sourcePath.generic_string();
        for (auto& row : batch.rows) {
            throwIfStopped(stopToken);
            if (!rowValue(row, ssaNumberKey).empty()) {
                requireValidImportedRow(row);
            }
        }
        for (auto& row : batch.rows) {
            throwIfStopped(stopToken);
            const auto number = rowValue(row, ssaNumberKey);
            if (number.empty()) {
                unkeyed_.emplace_back(std::move(row), source);
                continue;
            }
            auto& group = groups_[number];
            if (group.rows() > 0) {
                ++duplicateRows_;
            }
            // Journal a repeat of the same instant whose business fields differ. The text
            // follows the read order; the folded result does not.
            if (group.rows() > 0 &&
                domain::SsaImportPolicy::precedenceKey(row) == group.leadingPrecedence() &&
                group.hasRepeatDifference(row)) {
                const auto current = group.values();
                const auto merged = domain::SsaImportPolicy::merge(current, row);
                if (merged.repeatDiscarded) {
                    qInfo().noquote() << QString::fromStdString(
                        domain::SsaImportPolicy::describeConflict(current, row, merged));
                }
            }
            group.add(row, source, sourceName);
        }
    }

    ResolvedSsaImportRows SsaImportRunRows::resolve(const std::stop_token& stopToken) {
        ResolvedSsaImportRows resolved;
        resolved.duplicateRows = duplicateRows_;
        resolved.rows.reserve(groups_.size() + unkeyed_.size());
        // Each group is released once materialized: the run never holds both copies.
        for (auto entry = groups_.begin(); entry != groups_.end(); entry = groups_.erase(entry)) {
            throwIfStopped(stopToken);
            const auto& group = entry->second;
            resolved.rows.push_back(group.values());
            resolved.sources.push_back(group.winnerSource());
            resolved.newestOpen.push_back(group.newestOpen());
            resolved.oldestTerminal.push_back(group.oldestTerminal());
            if (domain::SsaImportPolicy::isOpenAfterTerminal(group.newestOpen(),
                                                             group.oldestTerminal())) {
                ++resolved.openAfterTerminalRows;
                qWarning().noquote() << QString::fromStdString(
                    domain::SsaImportPolicy::describeConflict(
                        group.oldestTerminal(), group.newestOpen(), {.openAfterTerminal = true}));
            }
        }
        for (auto& [row, source] : unkeyed_) {
            resolved.rows.push_back(std::move(row));
            resolved.sources.push_back(source);
            resolved.newestOpen.emplace_back();
            resolved.oldestTerminal.emplace_back();
        }
        unkeyed_.clear();
        return resolved;
    }

    ResolvedSsaImportRows SsaImportConflictResolver::resolveBySsaNumberKeepingUnkeyedRows(
        const std::vector<SsaImportBatch>& batches) const {
        SsaImportRunRows run;
        for (std::size_t source = 0; source < batches.size(); ++source) {
            run.add(batches[source], source);
        }
        return run.resolve();
    }

} // namespace ssa::infra::importing
