#include "infra/import/SamSpreadsheetAdapter.h"

#include "domain/WhitespaceTrim.h"

#include "domain/SsaImportPolicy.h"

#include <QDateTime>
#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <ranges>
#include <string_view>
#include <system_error>

namespace ssa::infra::importing {
    namespace {

        constexpr std::array<std::string_view, 11> kSamHeaders{
            "ssa_number",        "localization",   "description",     "issue_datetime",
            "emission_datetime", "emitter_sector", "executor_sector", "year_week",
            "situation_desc",    "process_status", "detail_present"};
        struct StatusDescription {
            std::string_view code;
            std::string_view description;
        };

        constexpr std::array<StatusDescription, 25> kKnownStatuses{
            StatusDescription{"AAD", "AGUARDANDO ATUALIZACAO DE DESENHOS"},
            StatusDescription{"AAT", "AGUARDANDO ATENDIMENTO DE TERCEIROS"},
            StatusDescription{"ACC", "AGUARDANDO CONDICOES CLIMATICAS"},
            StatusDescription{"ACS", "AGUARDANDO CONDICOES DO SISTEMA"},
            StatusDescription{"ADI", "AGUARDANDO APROVACAO DA DIVISAO NA EMISSAO"},
            StatusDescription{"ADM", "AGUARDANDO DEPARTAMENTO DE MANUTENCAO"},
            StatusDescription{"AIM", "AGUARDANDO DEPARTAMENTO DE ENGENHARIA DE MANUTENCAO"},
            StatusDescription{"AIP", "AGUARDANDO LIBERACAO DO EQUIPAMENTO"},
            StatusDescription{"AMP", "AGUARDANDO MANUTENCAO PERIODICA"},
            StatusDescription{"APG", "AGUARDANDO PROGRAMACAO"},
            StatusDescription{"APL", "AGUARDANDO PLANEJAMENTO"},
            StatusDescription{"APV", "AGUARDANDO PROVISIONAMENTO"},
            StatusDescription{"ASE", "AGUARDANDO APROVACAO DO SETOR NA EMISSAO"},
            StatusDescription{"ASI", "AGUARDANDO SERVICOS DE LABORATORIO"},
            StatusDescription{"ASO", "AGUARDANDO SERVICOS DE OFICINA"},
            StatusDescription{"SAD", "AGUARDANDO APROVACAO DA DIVISAO NA EXECUCAO"},
            StatusDescription{"SAS", "AGUARDANDO APROVACAO DO SETOR NA EXECUCAO"},
            StatusDescription{"SCA", "SERVICO CANCELADO"},
            StatusDescription{"SCD", "CANCELADA AGUARDANDO APROVACAO DA DIVISAO"},
            StatusDescription{"SCS", "CANCELADA AGUARDANDO APROVACAO DO SETOR"},
            StatusDescription{"SEE", "SERVICO EM EXECUCAO"},
            StatusDescription{"SES", "SERVICO COM EXECUCAO SIMPLES"},
            StatusDescription{"SPG", "SERVICO PROGRAMADO"},
            StatusDescription{"SRP", "SERVICO REPROGRAMADO"},
            StatusDescription{"STE", "SERVICO TERMINADO"}};

        void throwIfCanceled(const std::stop_token& stopToken) {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "SAM spreadsheet adaptation canceled");
            }
        }

        std::string uppercase(std::string value) {
            std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            return value;
        }

        bool rowHasContent(const std::vector<std::string>& row) {
            return std::ranges::any_of(
                row, [](const auto& value) { return !domain::trimWhitespace(value).empty(); });
        }

        std::string cell(const std::vector<std::string>& row, const std::size_t index) {
            return index < row.size() ? domain::trimWhitespace(row[index]) : std::string{};
        }

        bool hasExactHeaders(const std::vector<std::string>& row) {
            if (row.size() != kSamHeaders.size()) {
                return false;
            }
            for (std::size_t index = 0; index < row.size(); ++index) {
                if (domain::trimWhitespace(row[index]) != kSamHeaders[index]) {
                    return false;
                }
            }
            return true;
        }

        std::string normalizeDate(const std::string& value) {
            if (value.find('T') != std::string::npos) {
                const auto parsed =
                    QDateTime::fromString(QString::fromStdString(value), Qt::ISODate);
                if (parsed.isValid()) {
                    return parsed.toUTC()
                        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                        .toStdString();
                }
                return {};
            }
            return domain::SsaImportPolicy::normalizeSnapshotTimestamp(value);
        }

        std::string knownStatus(const std::string& value) {
            auto normalized = uppercase(domain::trimWhitespace(value));
            if (const auto separator = normalized.find(" - "); separator != std::string::npos) {
                normalized.resize(separator);
            }
            const auto status = std::ranges::find_if(kKnownStatuses, [&](const auto& entry) {
                return normalized == entry.code || normalized == entry.description;
            });
            return status == kKnownStatuses.end() ? std::string{} : std::string{status->code};
        }

        bool validYearWeek(const std::string& value) {
            if (value.size() != 6 || !std::ranges::all_of(value, [](const unsigned char ch) {
                    return std::isdigit(ch) != 0;
                })) {
                return false;
            }
            int week = 0;
            const auto [end, error] = std::from_chars(value.data() + 4, value.data() + 6, week);
            return error == std::errc{} && end == value.data() + 6 && week >= 1 && week <= 53;
        }

        std::optional<bool> detailPresent(const std::string& value) {
            const auto normalized = uppercase(domain::trimWhitespace(value));
            if (normalized == "TRUE" || normalized == "1") {
                return true;
            }
            if (normalized == "FALSE" || normalized == "0") {
                return false;
            }
            return std::nullopt;
        }

    } // namespace

    SamSpreadsheetAdaptResult
    SamSpreadsheetAdapter::adapt(const std::vector<SpreadsheetTable>& sheets,
                                 const ports::SamArtifact& artifact,
                                 const std::stop_token& stopToken) {
        throwIfCanceled(stopToken);
        if (sheets.size() != 1 || sheets.front().rows.empty() ||
            !hasExactHeaders(sheets.front().rows.front())) {
            return {.rejectionReason = "sam_schema_mismatch"};
        }
        const auto& source = sheets.front();
        SamSpreadsheetAdaptResult result;
        result.table.sourcePath = source.sourcePath;
        result.table.originalFilename = source.originalFilename;
        result.table.sourceModifiedTimestamp = source.sourceModifiedTimestamp;
        result.table.sourceCreatedTimestamp = source.sourceCreatedTimestamp;
        result.table.rows.push_back({"numero_ssa", "localizacao_codigo", "descricao_ssa",
                                     "data_cadastro", "setor_emissor", "setor_executor",
                                     "semana_cadastro", "situacao", "sistema_origem"});
        const auto physicalRows = static_cast<std::size_t>(
            std::count_if(source.rows.begin() + 1, source.rows.end(),
                          [](const auto& row) { return rowHasContent(row); }));
        if (physicalRows != artifact.manifestRows) {
            return {.rejectionReason = "sam_physical_count_mismatch"};
        }
        std::size_t detailRows = 0;
        std::size_t withoutDetailRows = 0;
        for (std::size_t rowIndex = 1; rowIndex < source.rows.size(); ++rowIndex) {
            throwIfCanceled(stopToken);
            const auto& row = source.rows[rowIndex];
            if (!rowHasContent(row)) {
                continue;
            }
            const auto number = domain::SsaImportPolicy::normalizeNumber(cell(row, 0));
            const auto description = cell(row, 2);
            const auto emissionDate = cell(row, 4);
            const auto issueDate = cell(row, 3);
            const auto date = normalizeDate(emissionDate.empty() ? issueDate : emissionDate);
            const auto executor = cell(row, 6);
            const auto yearWeek = cell(row, 7);
            auto status = knownStatus(cell(row, 8));
            if (status.empty()) {
                status = knownStatus(cell(row, 9));
            }
            const auto hasDetail = detailPresent(cell(row, 10));
            if (number.empty() || description.empty() || date.empty() ||
                executor != artifact.executorSector || !validYearWeek(yearWeek) || status.empty() ||
                !hasDetail) {
                return {.rejectionReason = "sam_row_contract_invalid"};
            }
            if (*hasDetail) {
                ++detailRows;
            } else {
                ++withoutDetailRows;
            }
            result.table.rows.push_back({number, cell(row, 1), description, date, cell(row, 5),
                                         executor, yearWeek, status, "SAM API"});
        }
        if (detailRows != artifact.detailRows || withoutDetailRows != artifact.withoutDetailRows ||
            detailRows + withoutDetailRows != artifact.manifestRows) {
            return {.rejectionReason = "sam_detail_count_mismatch"};
        }
        return result;
    }

} // namespace ssa::infra::importing
