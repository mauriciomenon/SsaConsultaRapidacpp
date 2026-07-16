#include "infra/sqlite/SqliteDerivadasPort.h"

#include "infra/import/DerivadasSourceReader.h"
#include "infra/import/LegacySpreadsheetConverter.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"
#include "qt/FilesystemPath.h"

#include <QTemporaryDir>

#include <sqlite3.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <system_error>

namespace ssa::infra::sqlite {

    namespace {

        ports::WorkflowResult succeeded(std::size_t fixedRecords) {
            return {ports::WorkflowStatus::Succeeded, "orphan derivation cleanup completed; " +
                                                          std::to_string(fixedRecords) +
                                                          " records fixed"};
        }

        ports::WorkflowResult canceled() {
            return {ports::WorkflowStatus::Canceled, "sqlite orphan derivation cleanup canceled"};
        }

        ports::WorkflowResult failed(std::string diagnostic) {
            return {ports::WorkflowStatus::Failed, "sqlite orphan derivation cleanup failed", false,
                    std::move(diagnostic)};
        }

        ports::WorkflowResult importRejected(std::string message) {
            return {ports::WorkflowStatus::Rejected, std::move(message)};
        }

        ports::WorkflowResult importCanceled() {
            return {ports::WorkflowStatus::Canceled, "derivadas import canceled"};
        }

        ports::WorkflowResult importFailed(std::string diagnostic) {
            return {ports::WorkflowStatus::Failed, "derivadas import failed", false,
                    std::move(diagnostic)};
        }

        std::string lowercaseExtension(const std::filesystem::path& path) {
            auto extension = path.extension().string();
            std::ranges::transform(extension, extension.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return extension;
        }

        importing::DerivadasSourceResult
        readLegacySource(const std::filesystem::path& source,
                         const importing::LegacySpreadsheetConverter& converter,
                         const std::stop_token& stopToken) {
            QTemporaryDir temporaryDirectory;
            if (!temporaryDirectory.isValid()) {
                return {importing::DerivadasSourceStatus::Failed,
                        {},
                        0,
                        "cannot create legacy XLS import directory"};
            }
            const auto temporaryRoot = qt::toFileSystemPath(temporaryDirectory.path());
            auto output = temporaryRoot / source.filename();
            output.replace_extension(".xlsx");
            const auto conversion = converter.convertToXlsx({source, output}, stopToken);
            if (conversion.status == importing::LegacySpreadsheetConversionStatus::Canceled) {
                return {
                    importing::DerivadasSourceStatus::Canceled, {}, 0, "derivadas import canceled"};
            }
            if (conversion.status ==
                importing::LegacySpreadsheetConversionStatus::ToolUnavailable) {
                return {importing::DerivadasSourceStatus::Rejected,
                        {},
                        0,
                        "Legacy XLS import requires LibreOffice",
                        conversion.diagnostic};
            }
            if (!conversion.ok() || !conversion.diagnostic.empty()) {
                return {importing::DerivadasSourceStatus::Failed,
                        {},
                        0,
                        conversion.message.empty() ? "legacy XLS conversion failed"
                                                   : conversion.message,
                        conversion.diagnostic};
            }
            auto result = importing::DerivadasSourceReader::read(conversion.outputPath, stopToken);
            if (!temporaryDirectory.remove()) {
                if (!result.diagnostic.empty()) {
                    result.diagnostic += "; ";
                }
                result.status = importing::DerivadasSourceStatus::Failed;
                result.message = "cannot clean legacy XLS import directory";
                result.diagnostic += "temporary XLS conversion output could not be removed";
            }
            return result;
        }

        importing::DerivadasSourceResult
        readSource(const std::filesystem::path& source,
                   const importing::LegacySpreadsheetConverter& converter,
                   const std::stop_token& stopToken) {
            return lowercaseExtension(source) == ".xls"
                       ? readLegacySource(source, converter, stopToken)
                       : importing::DerivadasSourceReader::read(source, stopToken);
        }

        ports::WorkflowResult sourceFailure(const importing::DerivadasSourceResult& result) {
            switch (result.status) {
            case importing::DerivadasSourceStatus::Rejected:
                return {ports::WorkflowStatus::Rejected, result.message, false, result.diagnostic};
            case importing::DerivadasSourceStatus::Canceled:
                return importCanceled();
            case importing::DerivadasSourceStatus::Failed:
                return importFailed(result.diagnostic.empty() ? result.message : result.diagnostic);
            case importing::DerivadasSourceStatus::Succeeded:
                break;
            }
            return importFailed("invalid derivadas source result");
        }

        ports::WorkflowResult importSucceeded(const std::size_t applied,
                                              const std::size_t duplicates,
                                              const std::size_t missingParents) {
            std::string message = "derivadas import completed; " + std::to_string(applied) +
                                  " applied; " + std::to_string(duplicates) + " duplicate";
            if (duplicates != 1) {
                message += 's';
            }
            message += "; " + std::to_string(missingParents) + " missing parent";
            if (missingParents != 1) {
                message += 's';
            }
            return {applied == 0 ? ports::WorkflowStatus::NoChanges
                                 : ports::WorkflowStatus::Succeeded,
                    std::move(message), missingParents > 0};
        }

        std::optional<ports::WorkflowResult>
        executeSyncSql(SqliteConnection& connection, const char* sql,
                       const std::atomic_bool* canceledByBusy) {
            char* error = nullptr;
            const int execRc = sqlite3_exec(connection.handle(), sql, nullptr, nullptr, &error);
            const std::string message = error == nullptr ? std::string{} : std::string{error};
            if (error != nullptr) {
                sqlite3_free(error);
            }
            const bool busyCanceled = (execRc == SQLITE_BUSY || execRc == SQLITE_LOCKED) &&
                                      canceledByBusy != nullptr &&
                                      canceledByBusy->load(std::memory_order_relaxed);
            if (execRc == SQLITE_INTERRUPT || busyCanceled) {
                return canceled();
            }
            if (execRc != SQLITE_OK) {
                return failed("sqlite orphan derivation cleanup failed: rc=" +
                              std::to_string(execRc) + " extended_rc=" +
                              std::to_string(sqlite3_extended_errcode(connection.handle())) +
                              " message=" + message);
            }
            return std::nullopt;
        }

    } // namespace

    SqliteDerivadasPort::SqliteDerivadasPort(
        std::filesystem::path databasePath,
        std::shared_ptr<importing::LegacySpreadsheetConverter> legacyConverter)
        : databasePath_(std::move(databasePath)), legacyConverter_(std::move(legacyConverter)) {}

    bool SqliteDerivadasPort::legacySpreadsheetConverterAvailable() const {
        return legacyConverter_ && legacyConverter_->available();
    }

    ports::WorkflowResult
    SqliteDerivadasPort::importDerivations(const ports::ImportDerivationsRequest& request,
                                           const std::stop_token stopToken) {
        try {
            if (request.files.empty()) {
                return importRejected("derivadas import requires at least one source");
            }
            if (!legacyConverter_) {
                return importFailed("legacy XLS converter is not configured");
            }

            importing::DerivadasEdgeMerger merger;
            for (const auto& source : request.files) {
                if (stopToken.stop_requested()) {
                    return importCanceled();
                }
                auto sourceResult = readSource(source, *legacyConverter_, stopToken);
                if (!sourceResult.ok()) {
                    return sourceFailure(sourceResult);
                }
                const auto mergeResult = merger.add(sourceResult.edges, stopToken);
                if (mergeResult.status == importing::DerivadasMergeStatus::Canceled) {
                    return importCanceled();
                }
                if (mergeResult.status == importing::DerivadasMergeStatus::Rejected) {
                    return importRejected(mergeResult.message);
                }
            }

            if (stopToken.stop_requested()) {
                return importCanceled();
            }

            SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
            SqliteBusyHandler busy(connection.handle(), stopToken);
            SqliteProgressHandler progress(connection.handle(), stopToken);
            SqliteWriteTransaction transaction(connection.handle(), busy.cancellationObserved());

            auto rollback = [&](ports::WorkflowResult result) {
                progress.disable();
                busy.disable();
                try {
                    transaction.rollback();
                } catch (const ports::OperationError& error) {
                    result.status = ports::WorkflowStatus::Failed;
                    result.message = "derivadas import failed";
                    if (!result.diagnostic.empty()) {
                        result.diagnostic += "; ";
                    }
                    result.diagnostic += error.diagnostic();
                }
                return result;
            };

            SqliteStatement exists(connection.handle(),
                                   "SELECT 1 FROM ssa_table WHERE numero_ssa = ? LIMIT 1",
                                   busy.cancellationObserved());
            const auto recordExists = [&](const std::string& number) {
                exists.bindTextOneBased(1, number);
                const bool found = exists.step();
                exists.resetAndClearBindings();
                return found;
            };

            const auto& parentByChild = merger.parentByChild();
            std::set<std::string> missingParents;
            for (const auto& [child, parent] : parentByChild) {
                if (stopToken.stop_requested()) {
                    return rollback(importCanceled());
                }
                if (!recordExists(child)) {
                    return rollback(importRejected("derivadas source references a missing child"));
                }
                if (!recordExists(parent)) {
                    missingParents.insert(parent);
                }
            }

            SqliteStatement update(connection.handle(),
                                   "UPDATE ssa_table SET derivada_de = ? WHERE numero_ssa = ? "
                                   "AND COALESCE(TRIM(derivada_de), '') <> ?",
                                   busy.cancellationObserved());
            std::size_t applied = 0;
            for (const auto& [child, parent] : parentByChild) {
                if (stopToken.stop_requested()) {
                    return rollback(importCanceled());
                }
                update.bindTextOneBased(1, parent);
                update.bindTextOneBased(2, child);
                update.bindTextOneBased(3, parent);
                update.executeAndReset();
                applied += static_cast<std::size_t>(sqlite3_changes(connection.handle()));
            }
            if (stopToken.stop_requested()) {
                return rollback(importCanceled());
            }
            try {
                transaction.commit();
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return rollback(importCanceled());
                }
                return rollback(importFailed(error.what()));
            } catch (const ports::OperationError& error) {
                return rollback(importFailed(error.diagnostic()));
            }
            return importSucceeded(applied, merger.duplicates(), missingParents.size());
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return importCanceled();
            }
            return importFailed(error.what());
        } catch (const ports::OperationError& error) {
            return importFailed(error.diagnostic());
        } catch (const std::exception& error) {
            return importFailed(error.what());
        }
    }

    ports::WorkflowResult
    SqliteDerivadasPort::cleanOrphanDerivations(const std::stop_token stopToken) {
        try {
            SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
            SqliteBusyHandler busy(connection.handle(), stopToken);
            SqliteProgressHandler progress(connection.handle(), stopToken);
            SqliteWriteTransaction transaction(connection.handle(), busy.cancellationObserved());

            auto rollback = [&](ports::WorkflowResult result) {
                progress.disable();
                busy.disable();
                try {
                    transaction.rollback();
                } catch (const ports::OperationError& error) {
                    result.status = ports::WorkflowStatus::Failed;
                    result.message = "sqlite orphan derivation cleanup failed";
                    result.diagnostic += "; " + error.diagnostic();
                }
                return result;
            };

            constexpr const char* operationSql =
                "WITH orphan_refs AS (\n"
                "    SELECT DISTINCT TRIM(derivada_de) AS orphan\n"
                "    FROM ssa_table\n"
                "    WHERE TRIM(COALESCE(derivada_de, '')) <> ''\n"
                "      AND NOT EXISTS (\n"
                "          SELECT 1 FROM ssa_table AS parents\n"
                "          WHERE parents.numero_ssa = TRIM(ssa_table.derivada_de)\n"
                "      )\n"
                ")\n"
                "UPDATE ssa_table\n"
                "SET derivada_de = NULL\n"
                "WHERE TRIM(COALESCE(derivada_de, '')) IN (SELECT orphan FROM orphan_refs)";
            if (auto result =
                    executeSyncSql(connection, operationSql, busy.cancellationObserved())) {
                return rollback(std::move(*result));
            }
            const auto fixedRecords =
                static_cast<std::size_t>(sqlite3_changes(connection.handle()));
            if (stopToken.stop_requested()) {
                return rollback(canceled());
            }
            try {
                transaction.commit();
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return rollback(canceled());
                }
                return rollback(failed(error.what()));
            } catch (const ports::OperationError& error) {
                return rollback(failed(error.diagnostic()));
            }
            return succeeded(fixedRecords);
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return canceled();
            }
            return failed(error.what());
        } catch (const ports::OperationError& error) {
            return failed(error.diagnostic());
        } catch (const std::exception& error) {
            return failed(error.what());
        }
    }

} // namespace ssa::infra::sqlite
