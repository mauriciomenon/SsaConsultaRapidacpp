#include "infra/import/SsaSpreadsheetMapper.h"

#include "domain/SsaImportPolicy.h"
#include "infra/import/SsaSpreadsheetHeaderCatalog.h"
#include "qt/FilesystemPath.h"

#include <QChar>
#include <QString>

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ssa::infra::importing {

    namespace {

        QString normalizedHeader(const std::string& value) {
            QString text = QString::fromStdString(value).normalized(QString::NormalizationForm_D);
            QString output;
            output.reserve(text.size());
            bool previousSpace = false;
            for (const auto ch : text) {
                if (ch.category() == QChar::Mark_NonSpacing) {
                    continue;
                }
                if (ch.isSpace()) {
                    if (!previousSpace) {
                        output.push_back(' ');
                    }
                    previousSpace = true;
                    continue;
                }
                output.push_back(ch.toLower());
                previousSpace = false;
            }
            return output.trimmed();
        }

        std::optional<std::string> canonicalColumn(const std::string& header) {
            return SsaSpreadsheetHeaderCatalog::canonicalColumnForHeader(
                normalizedHeader(header).toStdString());
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
                                                  HeaderColumnCache& cache) {
            std::size_t bestIndex = 0;
            std::size_t bestScore = 0;
            const auto limit = std::min<std::size_t>(table.rows.size(), 15);
            for (std::size_t rowIndex = 0; rowIndex < limit; ++rowIndex) {
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

        std::unordered_map<std::size_t, std::string>
        columnMapFromHeader(const std::vector<std::string>& header, HeaderColumnCache& cache) {
            std::unordered_map<std::size_t, std::string> mapped;
            std::unordered_set<std::string> used;
            for (std::size_t index = 0; index < header.size(); ++index) {
                auto column = cache.resolve(header[index]);
                if (!column || used.contains(*column)) {
                    continue;
                }
                used.insert(*column);
                mapped.emplace(index, *column);
            }
            return mapped;
        }

        std::string valueFor(const SsaImportRow& row, const std::string& key) {
            return rowValue(row, key);
        }

        bool hasRequiredColumns(const std::unordered_map<std::size_t, std::string>& columnByIndex) {
            static constexpr std::array<std::string_view, 3> required{"numero_ssa", "descricao_ssa",
                                                                      "data_cadastro"};
            return std::ranges::all_of(required, [&](const auto key) {
                return std::ranges::any_of(
                    columnByIndex, [key](const auto& column) { return column.second == key; });
            });
        }

    } // namespace

    SsaImportBatch SsaSpreadsheetMapper::map(const SpreadsheetTable& table) {
        SsaImportBatch batch;
        batch.sourcePath = table.sourcePath;
        HeaderColumnCache headerCache;
        const auto headerIndex = headerRowIndex(table, headerCache);
        if (!headerIndex) {
            batch.skippedRows = table.rows.size();
            return batch;
        }
        const auto columnByIndex = columnMapFromHeader(table.rows[*headerIndex], headerCache);
        batch.mappedColumns = columnByIndex.size();
        if (!hasRequiredColumns(columnByIndex)) {
            batch.mappingStatus = SpreadsheetMappingStatus::RequiredColumnsMissing;
            batch.skippedRows = table.rows.size() - *headerIndex - 1;
            return batch;
        }
        batch.mappingStatus = SpreadsheetMappingStatus::Mapped;
        for (std::size_t rowIndex = *headerIndex + 1; rowIndex < table.rows.size(); ++rowIndex) {
            SsaImportRow row;
            for (const auto& [columnIndex, columnKey] : columnByIndex) {
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
