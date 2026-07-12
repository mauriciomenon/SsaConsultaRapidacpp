#include "application/SsaExecutadasReportService.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace ssa::application {

    namespace {

        std::string trim(std::string value) {
            const auto first = std::find_if_not(
                value.begin(), value.end(), [](const unsigned char c) { return std::isspace(c); });
            const auto last =
                std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char c) {
                    return std::isspace(c);
                }).base();
            if (first >= last) {
                return {};
            }
            return {first, last};
        }

        std::string upper(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](const unsigned char c) { return std::toupper(c); });
            return value;
        }

        struct ReportKey {
            std::string group;
            std::string week;
            std::string person;

            bool operator<(const ReportKey& other) const {
                return std::tie(group, week, person) <
                       std::tie(other.group, other.week, other.person);
            }
        };

    } // namespace

    SsaExecutadasReportService::SsaExecutadasReportService(
        std::shared_ptr<query::SsaQueryService> queryService)
        : queryService_(std::move(queryService)) {}

    ExecutadasReportResult
    SsaExecutadasReportService::buildExecutadasReport(const domain::SsaPageRequest& request,
                                                      const bool byDivision,
                                                      const std::stop_token stopToken) const {
        if (!queryService_) {
            return {{}, false, "query service is unavailable"};
        }

        std::map<ReportKey, std::set<std::string>> grouped;
        const auto result = queryService_->readAll(
            request,
            [&](const domain::SsaRecord& record) {
                auto setor = upper(trim(std::string{record.valueOf("setor_executor")}));
                auto week = trim(std::string{record.valueOf("semana_executada")});
                auto person = trim(std::string{record.valueOf("responsavel_execucao")});
                const auto ssa = trim(std::string{record.valueOf("numero_ssa")});
                if (setor.empty() || week.empty() || ssa.empty()) {
                    return std::optional<std::string>{};
                }
                if (person.empty()) {
                    person = "-";
                }
                const auto group = byDivision ? setor.substr(0, 3) : setor;
                grouped[{group, week, person}].insert(ssa);
                return std::optional<std::string>{};
            },
            stopToken);
        if (!result.ok()) {
            return {{}, false, result.error};
        }

        ExecutadasReportResult output;
        output.ok = true;
        output.rows.reserve(grouped.size());
        for (const auto& [key, ssas] : grouped) {
            output.rows.push_back(ExecutadasReportRow{key.group, key.week, key.person,
                                                      static_cast<int>(ssas.size())});
        }
        return output;
    }

} // namespace ssa::application
