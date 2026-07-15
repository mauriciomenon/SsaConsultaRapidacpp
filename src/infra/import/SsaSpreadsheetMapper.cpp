#include "infra/import/SsaSpreadsheetMapper.h"

#include "domain/SsaImportPolicy.h"
#include "infra/import/SsaSpreadsheetHeaderCatalog.h"
#include "qt/FilesystemPath.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ssa::infra::importing {

    namespace {

        std::optional<std::string> canonicalColumn(const std::string& header) {
            return SsaSpreadsheetHeaderCatalog::canonicalColumnForHeader(header);
        }

        void throwIfMappingCanceled(const std::stop_token& stopToken) {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "spreadsheet mapping canceled");
            }
        }

        class HeaderColumnCache final {
          public:
            [[nodiscard]] std::optional<std::string> resolve(const std::string& header) {
                const auto found = cache_.find(header);
                if (found != cache_.end()) {
                    return found->second.empty() ? std::nullopt
                                                 : std::optional<std::string>{found->second};
                }
                const auto column = canonicalColumn(header);
                cache_.emplace(header, column.value_or(std::string{}));
                return column;
            }

          private:
            std::unordered_map<std::string, std::string> cache_;
        };

        std::string trimCopy(const std::string& value) {
            const auto begin = std::ranges::find_if_not(
                value, [](const unsigned char ch) { return std::isspace(ch) != 0; });
            const auto end =
                std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char ch) {
                    return std::isspace(ch) != 0;
                }).base();
            if (begin >= end) {
                return {};
            }
            return {begin, end};
        }

        std::optional<std::size_t> headerRowIndex(const SpreadsheetTable& table,
                                                  HeaderColumnCache& cache,
                                                  const std::stop_token& stopToken) {
            std::size_t bestIndex = 0;
            std::size_t bestScore = 0;
            const auto limit = std::min<std::size_t>(table.rows.size(), 15);
            for (std::size_t rowIndex = 0; rowIndex < limit; ++rowIndex) {
                throwIfMappingCanceled(stopToken);
                std::unordered_set<std::string> mapped;
                for (const auto& cell : table.rows[rowIndex]) {
                    if (auto column = cache.resolve(cell)) {
                        mapped.insert(*column);
                    }
                }
                const bool hasIdentity =
                    mapped.contains("numero_ssa") || mapped.contains("descricao_ssa");
                if (hasIdentity && mapped.size() > bestScore) {
                    bestScore = mapped.size();
                    bestIndex = rowIndex;
                }
            }
            if (bestScore < 3) {
                return std::nullopt;
            }
            return bestIndex;
        }

        using HeaderColumns = std::vector<std::pair<std::size_t, std::string>>;

        struct HeaderColumnMap {
            HeaderColumns columns;
            bool ambiguous = false;
        };

        std::span<const std::string_view> positionalFamily(const std::string_view canonical) {
            static constexpr std::array<std::string_view, 4> number{
                "numero_ssa", "numero_ssa_relacionada_1", "numero_ssa_relacionada_2",
                "numero_ssa_relacionada_3"};
            static constexpr std::array<std::string_view, 3> emitter{
                "setor_emissor", "setor_emissor_relacionado_1", "setor_emissor_relacionado_2"};
            static constexpr std::array<std::string_view, 3> executor{
                "setor_executor", "setor_executor_relacionado_1", "setor_executor_relacionado_2"};
            static constexpr std::array<std::string_view, 3> status{
                "situacao", "situacao_relacionada_1", "situacao_relacionada_2"};
            static constexpr std::array<std::string_view, 3> since{"desde", "desde_1", "desde_2"};
            static constexpr std::array<std::string_view, 3> until{"ate", "ate_1", "ate_2"};
            static constexpr std::array<std::string_view, 3> serial{"sn_retirado", "sn_instalado",
                                                                    "sn_extra"};
            if (canonical == "numero_ssa") {
                return number;
            }
            if (canonical == "setor_emissor") {
                return emitter;
            }
            if (canonical == "setor_executor") {
                return executor;
            }
            if (canonical == "situacao") {
                return status;
            }
            if (canonical == "desde") {
                return since;
            }
            if (canonical == "ate") {
                return until;
            }
            if (canonical == "sn") {
                return serial;
            }
            return {};
        }

        HeaderColumnMap columnMapFromHeader(const std::vector<std::string>& header,
                                            HeaderColumnCache& cache) {
            HeaderColumnMap mapped;
            std::unordered_set<std::string> seenCanonical;
            std::unordered_set<std::string> seenCanonicalHeader;
            std::unordered_map<std::string, std::size_t> repeatedHeaders;
            std::unordered_map<std::string, std::string> destinationOwner;
            for (std::size_t index = 0; index < header.size(); ++index) {
                auto column = cache.resolve(header[index]);
                if (!column) {
                    continue;
                }
                auto destination = *column;
                const auto family = positionalFamily(*column);
                if (!family.empty()) {
                    const bool firstCanonical = seenCanonical.insert(*column).second;
                    const bool firstHeader =
                        seenCanonicalHeader.insert(*column + '\n' + header[index]).second;
                    if (*column == "sn" || firstCanonical || !firstHeader) {
                        const auto occurrence = repeatedHeaders[*column]++;
                        if (occurrence >= family.size()) {
                            mapped.ambiguous = true;
                            return mapped;
                        }
                        destination = family[occurrence];
                    } else {
                        destination = family.front();
                    }
                }
                const auto [owner, inserted] = destinationOwner.try_emplace(destination, *column);
                if (!inserted && owner->second != *column) {
                    mapped.ambiguous = true;
                    return mapped;
                }
                mapped.columns.emplace_back(index, destination);
            }
            return mapped;
        }

        std::string valueFor(const SsaImportRow& row, const std::string& key) {
            return rowValue(row, key);
        }

        bool hasRequiredColumns(const HeaderColumns& columnByIndex) {
            static constexpr std::array<std::string_view, 3> required{"numero_ssa", "descricao_ssa",
                                                                      "data_cadastro"};
            return std::ranges::all_of(required, [&](const auto key) {
                return std::ranges::any_of(
                    columnByIndex, [key](const auto& column) { return column.second == key; });
            });
        }

    } // namespace

    SsaImportBatch SsaSpreadsheetMapper::map(const SpreadsheetTable& table,
                                             const std::stop_token& stopToken) {
        throwIfMappingCanceled(stopToken);
        SsaImportBatch batch;
        batch.sourcePath = table.sourcePath;
        HeaderColumnCache headerCache;
        const auto headerIndex = headerRowIndex(table, headerCache, stopToken);
        if (!headerIndex) {
            batch.skippedRows = table.rows.size();
            return batch;
        }
        const auto columnMap = columnMapFromHeader(table.rows[*headerIndex], headerCache);
        batch.mappedColumns = columnMap.columns.size();
        if (columnMap.ambiguous) {
            batch.mappingStatus = SpreadsheetMappingStatus::AmbiguousHeaders;
            batch.skippedRows = table.rows.size() - *headerIndex - 1;
            return batch;
        }
        if (!hasRequiredColumns(columnMap.columns)) {
            batch.mappingStatus = SpreadsheetMappingStatus::RequiredColumnsMissing;
            batch.skippedRows = table.rows.size() - *headerIndex - 1;
            return batch;
        }
        batch.headerRow = table.rows[*headerIndex];
        batch.mappingStatus = SpreadsheetMappingStatus::Mapped;
        for (std::size_t rowIndex = *headerIndex + 1; rowIndex < table.rows.size(); ++rowIndex) {
            throwIfMappingCanceled(stopToken);
            SsaImportRow row;
            for (const auto& [columnIndex, columnKey] : columnMap.columns) {
                if (columnIndex >= table.rows[rowIndex].size()) {
                    continue;
                }
                auto value = trimCopy(table.rows[rowIndex][columnIndex]);
                if (columnKey == "numero_ssa" || columnKey.starts_with("numero_ssa_relacionada")) {
                    value = domain::SsaImportPolicy::normalizeNumber(value);
                }
                if (!value.empty()) {
                    row.emplace(columnKey, value);
                }
            }
            const auto validation = domain::SsaImportPolicy::validateRow(row);
            if (validation != domain::SsaImportPolicy::RowValidationIssue::None) {
                ++batch.skippedRows;
                ++batch.invalidRows;
                switch (validation) {
                case domain::SsaImportPolicy::RowValidationIssue::InvalidNumber:
                    ++batch.invalidNumberRows;
                    break;
                case domain::SsaImportPolicy::RowValidationIssue::MissingDescription:
                    ++batch.invalidDescriptionRows;
                    break;
                case domain::SsaImportPolicy::RowValidationIssue::MissingDate:
                case domain::SsaImportPolicy::RowValidationIssue::InvalidDate:
                    ++batch.invalidDateRows;
                    break;
                case domain::SsaImportPolicy::RowValidationIssue::None:
                    break;
                }
                continue;
            }
            if (valueFor(row, "arquivo_origem").empty()) {
                row.emplace("arquivo_origem", table.originalFilename.empty()
                                                  ? qt::toUtf8(table.sourcePath.filename())
                                                  : table.originalFilename);
            }
            if (const auto sourceTimestamp = domain::SsaImportPolicy::normalizeSnapshotTimestamp(
                    table.sourceModifiedTimestamp);
                !sourceTimestamp.empty()) {
                if (valueFor(row, "data_arquivo_origem").empty()) {
                    row.emplace("data_arquivo_origem", sourceTimestamp);
                }
            }
            batch.rows.push_back(std::move(row));
        }
        return batch;
    }

} // namespace ssa::infra::importing
