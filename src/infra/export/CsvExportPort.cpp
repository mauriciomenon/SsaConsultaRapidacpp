#include "infra/export/CsvExportPort.h"

#include "domain/ColumnCatalog.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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

        void writeCsvValue(std::ofstream& output, const std::string_view value) {
            const bool spreadsheetFormula =
                !value.empty() && (value.front() == '=' || value.front() == '+' ||
                                   value.front() == '-' || value.front() == '@');
            if (!spreadsheetFormula && value.find_first_of("\",\n\r") == std::string::npos) {
                output << value;
                return;
            }
            output << '"';
            if (spreadsheetFormula) {
                output << '\'';
            }
            for (const char ch : value) {
                if (ch == '"') {
                    output << '"';
                }
                output << ch;
            }
            output << '"';
        }

        void writeCsvRow(std::ofstream& output, const domain::SsaRecord& record,
                         const std::vector<std::string>& columns) {
            for (std::size_t index = 0; index < columns.size(); ++index) {
                if (index > 0) {
                    output << ',';
                }
                writeCsvValue(output, record.valueOf(columns[index]));
            }
            output << '\n';
        }

        void writeCsvHeader(std::ofstream& output, const std::vector<std::string>& labels) {
            for (std::size_t index = 0; index < labels.size(); ++index) {
                if (index > 0) {
                    output << ',';
                }
                writeCsvValue(output, labels[index]);
            }
            output << '\n';
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

        ports::WorkflowResult writeFilteredRows(std::ofstream& output,
                                                const ports::ISsaRepository& repository,
                                                domain::SsaPageRequest query) {
            const auto columns = query.visibleColumns;
            const auto result = repository.readAll(
                query, [&output, columns](domain::SsaRecord row) -> std::optional<std::string> {
                    writeCsvRow(output, row, columns);
                    if (!output) {
                        return "failed while writing export output file";
                    }
                    return std::nullopt;
                });
            if (!result.ok()) {
                return failed(result.error);
            }
            if (!output) {
                return failed("failed while writing export output file");
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
    CsvExportPort::exportFilteredList(const ports::ExportFilteredListRequest& request) {
        try {
            const auto prepared = prepareExportRequest(request);
            if (!prepared.result.ok()) {
                return prepared.result;
            }

            // C++20 has no portable exclusive ofstream open mode; pre-existing files are rejected
            // above and packaging must run exports in user-chosen output directories.
            std::ofstream output(prepared.outputPath);
            if (!output.is_open() || !output) {
                return failed("failed to open export output file");
            }
            writeCsvHeader(output, prepared.headerLabels);
            if (!output) {
                return failed("failed while writing export output file");
            }
            return writeFilteredRows(output, *repository_, prepared.query);
        } catch (const std::exception& exc) {
            return failed(exc.what());
        }
    }

} // namespace ssa::infra::exporting
