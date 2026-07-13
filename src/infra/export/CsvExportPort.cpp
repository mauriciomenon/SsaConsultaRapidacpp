#include "infra/export/CsvExportPort.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "ports/ISsaRepository.h"
#include "qt/FilesystemPath.h"

#include <QFile>
#include <QTemporaryFile>

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ssa::infra::exporting {

    namespace {

        constexpr std::size_t kExportPageSize = 500;
        struct OutputPathValidation {
            ports::WorkflowResult result;
            std::filesystem::path normalizedPath;
        };

        struct PreparedExportRequest {
            ports::WorkflowResult result;
            std::filesystem::path outputPath;
            domain::SsaPageRequest query;
            std::vector<std::string> headerLabels;
        };

        bool isSpreadsheetFormulaPrefix(const std::string_view value) {
            if (value.empty()) {
                return false;
            }
            const auto first = static_cast<unsigned char>(value.front());
            if (first <= 0x1F || first == 0x7F || value.front() == '=' || value.front() == '+' ||
                value.front() == '-' || value.front() == '@') {
                return true;
            }
            constexpr std::array<std::string_view, 4> kFullWidthFormulaPrefixes{
                "\xEF\xBC\x9D", "\xEF\xBC\x8B", "\xEF\xBC\x8D", "\xEF\xBC\xA0"};
            return std::ranges::any_of(kFullWidthFormulaPrefixes, [value](const auto prefix) {
                return value.starts_with(prefix);
            });
        }

        void appendCsvValue(std::string& output, const std::string_view value) {
            const bool spreadsheetFormula = isSpreadsheetFormulaPrefix(value);
            if (!spreadsheetFormula && value.find_first_of("\",\n\r") == std::string::npos) {
                output.append(value);
                return;
            }
            output.push_back('"');
            if (spreadsheetFormula) {
                output.push_back('\'');
            }
            for (const char ch : value) {
                if (ch == '"') {
                    output.push_back('"');
                }
                output.push_back(ch);
            }
            output.push_back('"');
        }

        void buildCsvRow(std::string& output, const domain::SsaRecord& record,
                         const std::vector<std::string>& columns) {
            output.clear();
            for (std::size_t index = 0; index < columns.size(); ++index) {
                if (index > 0) {
                    output.push_back(',');
                }
                appendCsvValue(output, record.valueOf(columns[index]));
            }
            output.push_back('\n');
        }

        std::string csvHeader(const std::vector<std::string>& labels) {
            std::string output;
            for (std::size_t index = 0; index < labels.size(); ++index) {
                if (index > 0) {
                    output.push_back(',');
                }
                appendCsvValue(output, labels[index]);
            }
            output.push_back('\n');
            return output;
        }

        bool writeFully(QFile& output, const std::string_view value) {
            if (value.size() > static_cast<std::size_t>(std::numeric_limits<qint64>::max())) {
                return false;
            }
            qint64 offset = 0;
            const auto size = static_cast<qint64>(value.size());
            while (offset < size) {
                const qint64 written = output.write(value.data() + offset, size - offset);
                if (written <= 0) {
                    return false;
                }
                offset += written;
            }
            return true;
        }

        std::vector<std::string> exportColumns(std::vector<std::string> columns) {
            return domain::ColumnCatalog::visibleKeysOrDefault(std::move(columns));
        }

        ports::WorkflowResult rejected(std::string message) {
            return {ports::WorkflowStatus::Rejected, std::move(message)};
        }

        ports::WorkflowResult failed(std::string message) {
            return {ports::WorkflowStatus::Failed, std::move(message)};
        }

        ports::WorkflowResult canceled() {
            return {ports::WorkflowStatus::Canceled, "csv export canceled"};
        }

        bool samePathOrChildOf(const std::filesystem::path& path,
                               const std::filesystem::path& parent) {
            const auto relative = path.lexically_relative(parent);
            if (relative.empty()) {
                return path == parent;
            }
            return *relative.begin() != "..";
        }

        OutputPathValidation validateOutputPath(const std::filesystem::path& outputPath) {
            if (outputPath.empty()) {
                return {rejected("export output path is required"), {}};
            }
            const auto fileName = outputPath.filename();
            if (fileName.empty() || fileName == "." || fileName == "..") {
                return {rejected("export output file name is invalid"), {}};
            }
            if (outputPath.extension() != ".csv") {
                return {rejected("export output path must use .csv extension"), {}};
            }
            const auto parent = outputPath.parent_path().empty() ? std::filesystem::current_path()
                                                                 : outputPath.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) {
                return {rejected("export output directory does not exist"), {}};
            }
            const auto normalizedParent = std::filesystem::weakly_canonical(parent);
            if (normalizedParent.empty()) {
                return {rejected("export output directory is invalid"), {}};
            }
            const auto normalizedOutput =
                std::filesystem::weakly_canonical(normalizedParent / fileName);
            if (!samePathOrChildOf(normalizedOutput.parent_path(), normalizedParent)) {
                return {rejected("export output path escapes output directory"), {}};
            }
            if (std::filesystem::exists(normalizedOutput) &&
                std::filesystem::is_regular_file(normalizedOutput)) {
                return {rejected("export output file already exists"), {}};
            }
            if (std::filesystem::exists(normalizedOutput) &&
                std::filesystem::is_directory(normalizedOutput)) {
                return {rejected("export output path is a directory"), {}};
            }
            return {{ports::WorkflowStatus::Succeeded, {}}, normalizedOutput};
        }

        domain::SsaPageRequest prepareExportQuery(domain::SsaPageRequest query) {
            query.visibleColumns = exportColumns(std::move(query.visibleColumns));
            query.pageIndex = 0;
            query.pageSize = kExportPageSize;
            return query;
        }

        std::vector<std::string> headerLabelsFor(const std::vector<std::string>& columns) {
            std::vector<std::string> labels;
            labels.reserve(columns.size());
            for (const auto& key : columns) {
                const auto column = domain::ColumnCatalog::find(key);
                labels.push_back(column ? column->label : key);
            }
            return labels;
        }

        PreparedExportRequest
        prepareExportRequest(const ports::ExportFilteredListRequest& request) {
            const auto outputPath = validateOutputPath(request.outputPath);
            if (!outputPath.result.ok()) {
                return {outputPath.result, {}, {}, {}};
            }
            auto query = prepareExportQuery(request.query);
            auto labels = headerLabelsFor(query.visibleColumns);
            return {{ports::WorkflowStatus::Succeeded, {}},
                    outputPath.normalizedPath,
                    std::move(query),
                    std::move(labels)};
        }

        ports::WorkflowResult writeFilteredRows(QFile& output,
                                                const ports::ISsaRepository& repository,
                                                const domain::SsaPageRequest& query,
                                                const std::stop_token& stopToken) {
            const auto& columns = query.visibleColumns;
            std::string rowBuffer;
            const auto result = repository.readAll(
                query,
                [&output, &columns,
                 &rowBuffer](const domain::SsaRecord& row) -> std::optional<std::string> {
                    buildCsvRow(rowBuffer, row, columns);
                    if (!writeFully(output, rowBuffer)) {
                        return "failed while writing export output file";
                    }
                    return std::nullopt;
                },
                stopToken);
            if (!result.ok()) {
                return failed(result.error);
            }
            return {ports::WorkflowStatus::Succeeded,
                    "exported " + std::to_string(result.rowCount) + " rows"};
        }

    } // namespace

    CsvExportPort::CsvExportPort(std::shared_ptr<ports::ISsaRepository> repository)
        : repository_(std::move(repository)) {
        if (!repository_) {
            throw std::invalid_argument("repository is required");
        }
    }

    ports::WorkflowResult
    CsvExportPort::exportFilteredList(const ports::ExportFilteredListRequest& request,
                                      std::stop_token stopToken) {
        try {
            const auto prepared = prepareExportRequest(request);
            if (!prepared.result.ok()) {
                return prepared.result;
            }

            const auto outputPath = qt::toQString(prepared.outputPath);
            QTemporaryFile output(outputPath + QStringLiteral(".XXXXXX.tmp"));
            output.setAutoRemove(true);
            if (!output.open()) {
                return failed("failed to open export output file");
            }
            if (!writeFully(output, csvHeader(prepared.headerLabels))) {
                return failed("failed while writing export output file");
            }
            auto result = writeFilteredRows(output, *repository_, prepared.query, stopToken);
            if (!result.ok()) {
                return result;
            }
            if (!output.flush()) {
                return failed("failed while flushing export output file");
            }
            output.close();
            if (!output.rename(outputPath)) {
                return failed("export output file was created by another operation");
            }
            output.setAutoRemove(false);
            return result;
        } catch (const std::system_error& exc) {
            if (exc.code() == std::make_error_code(std::errc::operation_canceled)) {
                return canceled();
            }
            return failed(exc.what());
        } catch (const std::exception& exc) {
            return failed(exc.what());
        } catch (...) {
            return failed("unknown export failure");
        }
    }

} // namespace ssa::infra::exporting
