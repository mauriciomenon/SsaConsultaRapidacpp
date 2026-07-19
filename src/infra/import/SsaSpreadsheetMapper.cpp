#include "infra/import/SsaSpreadsheetMapper.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaImportPolicy.h"
#include "domain/WhitespaceTrim.h"
#include "infra/import/SsaSpreadsheetHeaderCatalog.h"
#include "qt/FilesystemPath.h"

#include <QChar>
#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
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

        std::string normalizeDeviationNumber(const std::string& value) {
            auto normalized = domain::trimWhitespace(value);
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            if (normalized.empty()) {
                return {};
            }
            const auto digitsOnly = [](const std::string_view candidate) {
                return !candidate.empty() &&
                       std::ranges::all_of(
                           candidate, [](const unsigned char ch) { return std::isdigit(ch) != 0; });
            };
            if (digitsOnly(normalized)) {
                return normalized;
            }
            if (normalized.starts_with("desvio")) {
                auto suffix = std::string_view{normalized}.substr(6);
                while (!suffix.empty() &&
                       std::isspace(static_cast<unsigned char>(suffix.front()))) {
                    suffix.remove_prefix(1);
                }
                if (!suffix.empty() && suffix.front() == '#') {
                    suffix.remove_prefix(1);
                    while (!suffix.empty() &&
                           std::isspace(static_cast<unsigned char>(suffix.front()))) {
                        suffix.remove_prefix(1);
                    }
                }
                if (digitsOnly(suffix)) {
                    return std::string{suffix};
                }
            }
            if (normalized == "sem desvio" || normalized == "sem desvios" ||
                normalized == "nenhum" || normalized == "nao") {
                return "0";
            }
            return {};
        }

        std::string normalizeReprogrammingNumber(const std::string& value) {
            const auto decomposed =
                QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()))
                    .normalized(QString::NormalizationForm_KD);
            QString folded;
            folded.reserve(decomposed.size());
            for (const auto character : decomposed) {
                if (character.category() != QChar::Mark_NonSpacing) {
                    folded.push_back(character.toLower());
                }
            }
            const auto normalized = folded.trimmed().toStdString();
            if (normalized == "final reschedule" || normalized == "reprogramacao final") {
                return {};
            }
            static constexpr std::array<std::string_view, 2> prefixes{"reschedule #",
                                                                      "reprogramacao #"};
            for (const auto prefix : prefixes) {
                if (!normalized.starts_with(prefix)) {
                    continue;
                }
                const auto suffix = std::string_view{normalized}.substr(prefix.size());
                if (!suffix.empty() && std::ranges::all_of(suffix, [](const unsigned char ch) {
                        return std::isdigit(ch) != 0;
                    })) {
                    return std::string{suffix};
                }
            }
            return value;
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

        enum class SnapshotHeaderKind {
            Other,
            Issue,
            Emission,
        };

        SnapshotHeaderKind snapshotHeaderKind(const std::string& header) {
            auto normalized = domain::trimWhitespace(header);
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            std::ranges::replace(normalized, '_', ' ');
            if (normalized.find("emission") != std::string::npos ||
                normalized.find("emissao") != std::string::npos ||
                normalized.find("emitida") != std::string::npos) {
                return SnapshotHeaderKind::Emission;
            }
            if (normalized.find("issue") != std::string::npos ||
                normalized.find("cadastro") != std::string::npos) {
                return SnapshotHeaderKind::Issue;
            }
            return SnapshotHeaderKind::Other;
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
            std::optional<std::size_t> fallbackTimestampColumn;
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
            std::unordered_map<std::string, std::size_t> destinationIndexes;
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
                const auto [owner, inserted] =
                    destinationIndexes.try_emplace(destination, mapped.columns.size());
                if (!inserted) {
                    if (destination != "data_cadastro") {
                        mapped.ambiguous = true;
                        return mapped;
                    }
                    const auto existingIndex = mapped.columns[owner->second].first;
                    const auto existingKind = snapshotHeaderKind(header[existingIndex]);
                    const auto incomingKind = snapshotHeaderKind(header[index]);
                    if (existingKind == incomingKind) {
                        mapped.ambiguous = true;
                        return mapped;
                    }
                    const auto recordFallback = [&](const std::size_t fallbackIndex) {
                        if (mapped.fallbackTimestampColumn &&
                            *mapped.fallbackTimestampColumn != fallbackIndex) {
                            mapped.ambiguous = true;
                            return false;
                        }
                        mapped.fallbackTimestampColumn = fallbackIndex;
                        return true;
                    };
                    const auto incomingPreferred = incomingKind == SnapshotHeaderKind::Emission ||
                                                   (incomingKind == SnapshotHeaderKind::Issue &&
                                                    existingKind == SnapshotHeaderKind::Other);
                    if (incomingPreferred) {
                        if (!recordFallback(existingIndex)) {
                            return mapped;
                        }
                        mapped.columns[owner->second] = {index, destination};
                    } else if (!recordFallback(index)) {
                        return mapped;
                    }
                    continue;
                }
                mapped.columns.emplace_back(index, destination);
            }
            return mapped;
        }

        std::string valueFor(const SsaImportRow& row, const std::string& key) {
            return rowValue(row, key);
        }

        bool isLegacyIncompleteSummaryRow(const SpreadsheetTable& table, const SsaImportRow& row) {
            const auto filename = table.originalFilename.empty()
                                      ? qt::toUtf8(table.sourcePath.filename())
                                      : table.originalFilename;
            auto normalizedFilename = filename;
            std::ranges::transform(normalizedFilename, normalizedFilename.begin(),
                                   [](const unsigned char character) {
                                       return static_cast<char>(std::tolower(character));
                                   });
            if (normalizedFilename.find("todas as ssas") == std::string::npos ||
                !valueFor(row, "descricao_ssa").empty() ||
                !valueFor(row, "data_cadastro").empty() ||
                valueFor(row, "semana_cadastro").empty()) {
                return false;
            }
            const auto status = valueFor(row, "situacao");
            return status == "SCC" || status == "ADI" || status == "ASE";
        }

        bool hasRequiredColumns(const HeaderColumns& columnByIndex) {
            const auto hasColumn = [&](const std::string_view key) {
                return std::ranges::any_of(
                    columnByIndex, [key](const auto& column) { return column.second == key; });
            };
            return std::ranges::all_of(domain::ColumnCatalog::requiredSchemaColumns(), hasColumn);
        }

        bool isRelationCategoryHeader(const std::string& header) {
            const auto decomposed =
                QString::fromUtf8(header.data(), static_cast<qsizetype>(header.size()))
                    .normalized(QString::NormalizationForm_KD);
            QString folded;
            folded.reserve(decomposed.size());
            for (const auto character : decomposed) {
                if (character.category() != QChar::Mark_NonSpacing) {
                    folded.push_back(character.toLower());
                }
            }
            return folded.trimmed() == "categoria" || folded.trimmed() == "category";
        }

        bool isSamApiReport(const std::vector<std::string>& header) {
            static constexpr std::array<std::string_view, 11> expected{
                "ssa_number",        "localization",   "description",     "issue_datetime",
                "emission_datetime", "emitter_sector", "executor_sector", "year_week",
                "situation_desc",    "process_status", "detail_present"};
            return header.size() == expected.size() &&
                   std::ranges::equal(header, expected,
                                      [](const std::string& actual, const std::string_view wanted) {
                                          return domain::trimWhitespace(actual) == wanted;
                                      });
        }

        bool isDerivationRelationReport(const HeaderColumns& columnByIndex,
                                        const std::vector<std::string>& header) {
            const auto hasColumn = [&](const std::string_view key) {
                return std::ranges::any_of(
                    columnByIndex, [key](const auto& column) { return column.second == key; });
            };
            const bool multiSsaRelation = hasColumn("relacao") && hasColumn("numero_ssa") &&
                                          hasColumn("numero_ssa_relacionada_1") &&
                                          hasColumn("numero_ssa_relacionada_2");
            const bool compactRelation =
                hasColumn("numero_ssa") && hasColumn("localizacao_codigo") &&
                hasColumn("setor_emissor") && hasColumn("setor_executor") &&
                hasColumn("situacao") && !hasColumn("descricao_ssa") &&
                !hasColumn("data_cadastro") &&
                std::ranges::any_of(header, isRelationCategoryHeader);
            return multiSsaRelation || compactRelation;
        }

    } // namespace

    SsaImportBatch SsaSpreadsheetMapper::map(const SpreadsheetTable& table,
                                             const std::stop_token& stopToken) {
        return map(table, stopToken, {});
    }

    SsaImportBatch SsaSpreadsheetMapper::map(const SpreadsheetTable& table,
                                             const std::stop_token& stopToken,
                                             const MappingCheckpoint& afterFirstRowMapped) {
        throwIfMappingCanceled(stopToken);
        SsaImportBatch batch;
        batch.sourcePath = table.sourcePath;
        HeaderColumnCache headerCache;
        const bool hasExternalHeader = !table.headerRow.empty();
        const auto headerIndex = hasExternalHeader ? std::optional<std::size_t>{0}
                                                   : headerRowIndex(table, headerCache, stopToken);
        if (!headerIndex) {
            batch.skippedRows = table.rows.size();
            return batch;
        }
        const auto& header = hasExternalHeader ? table.headerRow : table.rows[*headerIndex];
        if (isSamApiReport(header)) {
            batch.mappingStatus = SpreadsheetMappingStatus::HeaderNotRecognized;
            batch.skippedRows =
                hasExternalHeader ? table.rows.size() : table.rows.size() - *headerIndex - 1;
            return batch;
        }
        const auto columnMap = columnMapFromHeader(header, headerCache);
        batch.mappedColumns = columnMap.columns.size();
        if (columnMap.ambiguous) {
            batch.mappingStatus = SpreadsheetMappingStatus::AmbiguousHeaders;
            batch.skippedRows =
                hasExternalHeader ? table.rows.size() : table.rows.size() - *headerIndex - 1;
            return batch;
        }
        if (isDerivationRelationReport(columnMap.columns, header)) {
            batch.mappingStatus = SpreadsheetMappingStatus::HeaderNotRecognized;
            batch.skippedRows =
                hasExternalHeader ? table.rows.size() : table.rows.size() - *headerIndex - 1;
            return batch;
        }
        if (!hasRequiredColumns(columnMap.columns)) {
            batch.mappingStatus = SpreadsheetMappingStatus::RequiredColumnsMissing;
            batch.skippedRows =
                hasExternalHeader ? table.rows.size() : table.rows.size() - *headerIndex - 1;
            return batch;
        }
        batch.headerRow = header;
        batch.mappingStatus = SpreadsheetMappingStatus::Mapped;
        const std::size_t firstDataRow = hasExternalHeader ? 0 : *headerIndex + 1;
        bool checkpointCalled = false;
        for (std::size_t rowIndex = firstDataRow; rowIndex < table.rows.size(); ++rowIndex) {
            throwIfMappingCanceled(stopToken);
            SsaImportRow row;
            for (const auto& [columnIndex, columnKey] : columnMap.columns) {
                if (columnIndex >= table.rows[rowIndex].size()) {
                    continue;
                }
                auto value = domain::trimWhitespace(table.rows[rowIndex][columnIndex]);
                if (columnKey == "numero_ssa" || columnKey.starts_with("numero_ssa_relacionada")) {
                    value = domain::SsaImportPolicy::normalizeNumber(value);
                } else if (columnKey == "numero_desvios") {
                    value = normalizeDeviationNumber(value);
                } else if (columnKey == "num_reprogramacoes") {
                    value = normalizeReprogrammingNumber(value);
                } else if (const auto* column = domain::ColumnCatalog::find(columnKey);
                           column != nullptr && column->type == domain::ColumnType::DateText) {
                    const auto normalized = domain::SsaImportPolicy::normalizeDateText(value);
                    if (!normalized.empty() || value.empty()) {
                        value = normalized;
                    }
                }
                if (!value.empty()) {
                    row.emplace(columnKey, value);
                }
            }
            if (!checkpointCalled && afterFirstRowMapped) {
                checkpointCalled = true;
                afterFirstRowMapped();
                throwIfMappingCanceled(stopToken);
            }
            if (valueFor(row, "data_cadastro").empty() && columnMap.fallbackTimestampColumn) {
                const auto fallbackColumn = *columnMap.fallbackTimestampColumn;
                if (fallbackColumn < table.rows[rowIndex].size()) {
                    const auto fallback = domain::SsaImportPolicy::normalizeSnapshotTimestamp(
                        domain::trimWhitespace(table.rows[rowIndex][fallbackColumn]));
                    if (!fallback.empty()) {
                        row.emplace("data_cadastro", fallback);
                    }
                }
            }
            if (row.empty() ||
                (valueFor(row, "numero_ssa").empty() && valueFor(row, "descricao_ssa").empty())) {
                continue;
            }
            if (isLegacyIncompleteSummaryRow(table, row)) {
                ++batch.skippedRows;
                continue;
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
                case domain::SsaImportPolicy::RowValidationIssue::MissingWeek:
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
            if (const auto sourceCreatedTimestamp =
                    domain::SsaImportPolicy::normalizeSnapshotTimestamp(
                        table.sourceCreatedTimestamp);
                !sourceCreatedTimestamp.empty() && valueFor(row, "data_criacao_arquivo").empty()) {
                row.emplace("data_criacao_arquivo", sourceCreatedTimestamp);
            }
            batch.rows.push_back(std::move(row));
        }
        return batch;
    }

} // namespace ssa::infra::importing
