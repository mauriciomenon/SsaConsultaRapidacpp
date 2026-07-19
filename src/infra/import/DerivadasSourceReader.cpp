#include "infra/import/DerivadasSourceReader.h"

#include "domain/SsaImportPolicy.h"
#include "infra/import/SpreadsheetTable.h"
#include "infra/import/XlsxWorkbookReader.h"

#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace ssa::infra::importing {

    namespace {

        using Rows = std::vector<std::vector<std::string>>;

        constexpr std::array<std::string_view, 6> kParentAliases{
            "parent", "parentssa", "ssamae", "ssapai", "numerossamae", "numeromae"};
        constexpr std::array<std::string_view, 7> kChildAliases{
            "child",          "childssa",    "ssafilha", "ssaderivada",
            "numerossafilha", "numerofilha", "numerossa"};

        void throwIfCanceled(const std::stop_token& stopToken) {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
        }

        std::string normalizedHeader(const std::string& value) {
            const auto decomposed =
                QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()))
                    .normalized(QString::NormalizationForm_D);
            QString normalized;
            normalized.reserve(decomposed.size());
            for (const auto character : decomposed) {
                const auto category = character.category();
                if (category == QChar::Mark_NonSpacing ||
                    category == QChar::Mark_SpacingCombining || category == QChar::Mark_Enclosing) {
                    continue;
                }
                if (character.isLetterOrNumber()) {
                    normalized.append(character.toLower());
                }
            }
            return normalized.toUtf8().toStdString();
        }

        bool matches(const std::string& header, const std::span<const std::string_view> aliases) {
            return std::ranges::find(aliases, header) != aliases.end();
        }

        std::string cell(const std::vector<std::string>& row, const std::size_t index) {
            return index < row.size() ? row[index] : std::string{};
        }

        DerivadasSourceResult rejected(std::string message, const std::size_t inputRows = 0) {
            return {DerivadasSourceStatus::Rejected, {}, inputRows, std::move(message)};
        }

        std::optional<std::vector<std::string>>
        parseDelimitedLine(const std::string& line, const char delimiter,
                           const std::stop_token& stopToken,
                           const std::function<void()>& afterFirstParsingChunk) {
            std::vector<std::string> fields;
            std::string field;
            bool quoted = false;
            for (std::size_t index = 0; index < line.size(); ++index) {
                if ((index & 4095U) == 0U) {
                    if (index == 4096U && afterFirstParsingChunk) {
                        afterFirstParsingChunk();
                    }
                    throwIfCanceled(stopToken);
                }
                const char ch = line[index];
                if (ch == '"') {
                    if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
                        field.push_back('"');
                        ++index;
                    } else {
                        quoted = !quoted;
                    }
                    continue;
                }
                if (ch == delimiter && !quoted) {
                    fields.push_back(std::move(field));
                    field.clear();
                    continue;
                }
                field.push_back(ch);
            }
            if (quoted) {
                return std::nullopt;
            }
            fields.push_back(std::move(field));
            return fields;
        }

        struct DelimitedReadResult {
            Rows rows;
            std::optional<DerivadasSourceResult> error;
        };

        DelimitedReadResult readDelimited(const std::filesystem::path& source, const char delimiter,
                                          const std::stop_token& stopToken,
                                          const std::function<void()>& afterFirstParsingChunk) {
            std::ifstream input(source, std::ios::binary);
            if (!input.is_open()) {
                return {{},
                        DerivadasSourceResult{
                            DerivadasSourceStatus::Failed, {}, 0, "cannot read derivadas source"}};
            }
            Rows rows;
            std::string line;
            while (std::getline(input, line)) {
                throwIfCanceled(stopToken);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                auto parsed =
                    parseDelimitedLine(line, delimiter, stopToken, afterFirstParsingChunk);
                if (!parsed) {
                    return {{},
                            rejected("derivadas source has malformed delimited data", rows.size())};
                }
                rows.push_back(std::move(*parsed));
            }
            if (input.bad()) {
                return {{},
                        DerivadasSourceResult{DerivadasSourceStatus::Failed,
                                              {},
                                              rows.size(),
                                              "cannot read derivadas source"}};
            }
            return {std::move(rows), std::nullopt};
        }

        struct HeaderMapping {
            std::size_t row = 0;
            std::size_t parent = 0;
            std::size_t child = 0;
            std::size_t relation = 0;
        };

        std::optional<HeaderMapping> standardHeader(const Rows& rows) {
            const auto rowLimit = std::min<std::size_t>(rows.size(), 15);
            for (std::size_t rowIndex = 0; rowIndex < rowLimit; ++rowIndex) {
                std::vector<std::size_t> parents;
                std::vector<std::size_t> children;
                for (std::size_t index = 0; index < rows[rowIndex].size(); ++index) {
                    const auto header = normalizedHeader(rows[rowIndex][index]);
                    if (matches(header, kParentAliases)) {
                        parents.push_back(index);
                    }
                    if (matches(header, kChildAliases)) {
                        children.push_back(index);
                    }
                }
                if (parents.size() == 1 && children.size() == 1 && parents[0] != children[0]) {
                    return HeaderMapping{rowIndex, parents[0], children[0], 0};
                }
            }
            return std::nullopt;
        }

        std::vector<HeaderMapping> visualHeaders(const Rows& rows) {
            const auto rowLimit = std::min<std::size_t>(rows.size(), 15);
            for (std::size_t rowIndex = 0; rowIndex < rowLimit; ++rowIndex) {
                std::vector<std::size_t> numbers;
                std::vector<std::size_t> relations;
                for (std::size_t index = 0; index < rows[rowIndex].size(); ++index) {
                    const auto header = normalizedHeader(rows[rowIndex][index]);
                    if (header == "numerodassa") {
                        numbers.push_back(index);
                    } else if (header == "relacao" || header == "relation") {
                        relations.push_back(index);
                    }
                }
                std::vector<HeaderMapping> mappings;
                for (const auto relation : relations) {
                    std::optional<std::size_t> left;
                    std::optional<std::size_t> right;
                    for (const auto number : numbers) {
                        if (number < relation) {
                            left = number;
                        } else if (number > relation && !right) {
                            right = number;
                        }
                    }
                    if (left && right) {
                        mappings.push_back({rowIndex, *right, *left, relation});
                    }
                }
                if (!mappings.empty()) {
                    return mappings;
                }
            }
            return {};
        }

        std::optional<DerivadasSourceResult> appendEdge(DerivadasSourceResult& result,
                                                        const std::string& parentValue,
                                                        const std::string& childValue) {
            const auto parent = domain::SsaImportPolicy::normalizeNumber(parentValue);
            const auto child = domain::SsaImportPolicy::normalizeNumber(childValue);
            if (parent.empty() && child.empty()) {
                return std::nullopt;
            }
            ++result.inputRows;
            if (parent.empty()) {
                return rejected("derivadas source contains an invalid parent", result.inputRows);
            }
            if (child.empty()) {
                return rejected("derivadas source contains an invalid child", result.inputRows);
            }
            if (parent == child) {
                return rejected("derivadas source contains a self-loop", result.inputRows);
            }
            result.edges.push_back({parent, child});
            return std::nullopt;
        }

        DerivadasSourceResult parseTables(const std::vector<SpreadsheetTable>& tables,
                                          const std::stop_token& stopToken) {
            DerivadasSourceResult result{DerivadasSourceStatus::Succeeded};
            bool recognized = false;
            for (const auto& table : tables) {
                throwIfCanceled(stopToken);
                if (const auto header = standardHeader(table.rows)) {
                    recognized = true;
                    for (std::size_t row = header->row + 1; row < table.rows.size(); ++row) {
                        throwIfCanceled(stopToken);
                        if (auto error = appendEdge(result, cell(table.rows[row], header->parent),
                                                    cell(table.rows[row], header->child))) {
                            return *error;
                        }
                    }
                    continue;
                }
                const auto mappings = visualHeaders(table.rows);
                if (mappings.empty()) {
                    continue;
                }
                recognized = true;
                for (std::size_t row = mappings.front().row + 1; row < table.rows.size(); ++row) {
                    throwIfCanceled(stopToken);
                    for (const auto& mapping : mappings) {
                        const auto parentValue = cell(table.rows[row], mapping.parent);
                        const auto childValue = cell(table.rows[row], mapping.child);
                        const auto relationValue = cell(table.rows[row], mapping.relation);
                        if (domain::SsaImportPolicy::normalizeNumber(parentValue).empty() &&
                            normalizedHeader(relationValue).empty()) {
                            continue;
                        }
                        if (auto error = appendEdge(result, parentValue, childValue)) {
                            return *error;
                        }
                    }
                }
            }
            if (!recognized) {
                return rejected("derivadas source schema was not recognized");
            }
            if (result.edges.empty()) {
                return rejected("derivadas source contains no valid edges", result.inputRows);
            }
            return result;
        }

        std::string lowercaseExtension(const std::filesystem::path& source) {
            auto extension = source.extension().string();
            std::ranges::transform(extension, extension.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return extension;
        }

    } // namespace

    DerivadasMergeResult DerivadasEdgeMerger::add(const std::span<const DerivationEdge> edges,
                                                  const std::stop_token& stopToken) {
        return add(edges, stopToken, {});
    }

    DerivadasMergeResult
    DerivadasEdgeMerger::add(const std::span<const DerivationEdge> edges,
                             const std::stop_token& stopToken,
                             const std::function<void()>& afterFirstEdgeMerged) {
        bool firstEdgeMerged = false;
        for (const auto& edge : edges) {
            if (stopToken.stop_requested()) {
                return {DerivadasMergeStatus::Canceled, "derivadas import canceled"};
            }
            if (!uniqueEdges_.emplace(edge.parent, edge.child).second) {
                ++duplicates_;
                continue;
            }
            const auto [current, inserted] = parentByChild_.emplace(edge.child, edge.parent);
            if (!inserted && current->second != edge.parent) {
                return {DerivadasMergeStatus::Rejected,
                        "derivadas source contains multiple parents for one child"};
            }
            if (!firstEdgeMerged) {
                firstEdgeMerged = true;
                if (afterFirstEdgeMerged) {
                    afterFirstEdgeMerged();
                    if (stopToken.stop_requested()) {
                        return {DerivadasMergeStatus::Canceled, "derivadas import canceled"};
                    }
                }
            }
        }
        return {DerivadasMergeStatus::Succeeded, {}};
    }

    const std::map<std::string, std::string>& DerivadasEdgeMerger::parentByChild() const {
        return parentByChild_;
    }

    std::size_t DerivadasEdgeMerger::duplicates() const {
        return duplicates_;
    }

    DerivadasSourceResult DerivadasSourceReader::read(const std::filesystem::path& source,
                                                      const std::stop_token& stopToken) {
        return read(source, stopToken, {});
    }

    DerivadasSourceResult
    DerivadasSourceReader::read(const std::filesystem::path& source,
                                const std::stop_token& stopToken,
                                const std::function<void()>& afterFirstParsingChunk) {
        try {
            throwIfCanceled(stopToken);
            std::error_code error;
            if (!std::filesystem::is_regular_file(source, error) || error) {
                return rejected("derivadas source is not a regular file");
            }
            const auto extension = lowercaseExtension(source);
            if (extension == ".csv" || extension == ".txt" || extension == ".tsv") {
                auto delimited = readDelimited(source, extension == ".tsv" ? '\t' : ',', stopToken,
                                               afterFirstParsingChunk);
                if (delimited.error) {
                    return std::move(*delimited.error);
                }
                SpreadsheetTable table;
                table.sourcePath = source;
                table.rows = std::move(delimited.rows);
                return parseTables({std::move(table)}, stopToken);
            }
            if (extension == ".xlsx" || extension == ".xlsm") {
                return parseTables(XlsxWorkbookReader::readSheets(source, stopToken), stopToken);
            }
            return rejected("derivadas source format is not supported");
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return {DerivadasSourceStatus::Canceled, {}, 0, "derivadas import canceled"};
            }
            return {
                DerivadasSourceStatus::Failed, {}, 0, "cannot read derivadas source", error.what()};
        } catch (const std::exception& error) {
            return {
                DerivadasSourceStatus::Failed, {}, 0, "cannot read derivadas source", error.what()};
        }
    }

} // namespace ssa::infra::importing
