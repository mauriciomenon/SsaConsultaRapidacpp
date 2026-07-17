#include "domain/ColumnCatalog.h"

#include "domain/TextFilterToken.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ssa::domain {

    namespace {

        using enum ColumnType;

        constexpr std::string_view kStatusColumnKey = "situacao";
        constexpr std::string_view kExecutorColumnKey = "setor_executor";
        constexpr std::string_view kDerivationColumnKey = "derivada_de";
        constexpr std::string_view kDerivedCountColumnKey = "qtd_derivadas";
        constexpr std::array<std::string_view, 26> kStatusShortcutCodes{{
            "AAD", "AAT", "ACC", "ACS", "ADI", "ADM", "AIM", "ALE", "AMP",
            "APG", "APL", "APV", "ASE", "ASL", "ASO", "SAD", "SAS", "SCA",
            "SCC", "SCD", "SCS", "SEE", "SES", "SPG", "SRP", "STE",
        }};
        constexpr std::string_view kDownloadableStatusFilterExpression = "!SAD,!SCA,!SES,!STE";
        constexpr std::array<std::string_view, 3> kExcludedStatusCodes{"SCA", "SES", "STE"};
        constexpr std::array<std::string_view, 3> kWeekColumnKeys{
            "semana_cadastro", "semana_programada", "semana_executada"};
        constexpr std::array<std::string_view, 2> kReprogrammingColumnKeys{
            "num_reprogramacoes", "total_de_reprogramacoes"};
        constexpr std::array<std::string_view, 15> kDefaultVisibleColumnKeys{{
            "numero_ssa",
            "situacao",
            "localizacao_codigo",
            "setor_emissor",
            "setor_executor",
            "qtd_derivadas",
            "descricao_ssa",
            "semana_cadastro",
            "solicitante",
            "derivada_de",
            "responsavel_programacao",
            "responsavel_execucao",
            "semana_programada",
            "semana_executada",
            "descricao_execucao",
        }};
        constexpr std::array<std::string_view, 11> kAdvancedFilterKeys{{
            "setor_emissor",
            "setor_executor",
            "situacao",
            "solicitante",
            "responsavel_programacao",
            "responsavel_execucao",
            "localizacao_codigo",
            "status_execucao_prazo",
            "anomalia",
            "situacao_espera",
            "situacao_da_parcial",
        }};
        constexpr std::string_view kStatusLastSortCode = "STE";

        const std::array<ColumnDef, 85> kColumns{{
            {"id", Integer, false},
            {"numero_ssa", Text, true},
            {"situacao", Text, true},
            {"localizacao_codigo", Text, true},
            {"setor_emissor", Text, true},
            {"setor_executor", Text, true},
            {"qtd_derivadas", Integer, false},
            {"descricao_ssa", Text, true},
            {"data_cadastro", DateText, false},
            {"derivada_de", Text, true},
            {"semana_cadastro", Integer, true},
            {"descricao_localizacao", Text, true},
            {"equipamento", Text, true},
            {"descricao_execucao", Text, true},
            {"solicitante", Text, true},
            {"responsavel_programacao", Text, true},
            {"responsavel_execucao", Text, true},
            {"servico_origem", Text, true},
            {"sistema_origem", Text, true},
            {"arquivo_origem", Text, false},
            {"data_planilha", DateText, false},
            {"grau_prioridade_emissao", Text, true},
            {"grau_prioridade_planejamento", Text, true},
            {"execucao_simples", Text, false},
            {"semana_programada", Integer, true},
            {"prazo_limite", DateText, false},
            {"status_execucao_prazo", Text, false},
            {"tempo_disponivel", Text, false},
            {"data_limite", DateText, false},
            {"tempo_excedido", Text, false},
            {"desde", Text, false},
            {"tempo_total", Text, false},
            {"desde_1", Text, false},
            {"total_tempo_tpe_planejado", Text, false},
            {"total_tempo_tex_planejado", Text, false},
            {"total_tempo_tpo_planejado", Text, false},
            {"total_horas_programadas", Text, false},
            {"total_tempo_tpe_executada", Text, false},
            {"semana_executada", Integer, true},
            {"num_reprogramacoes", Integer, false},
            {"execucao_parcial", Text, true},
            {"anomalia", Text, true},
            {"registros_espera", Text, false},
            {"num_reprobaciones", Integer, false},
            {"situacao_espera", Text, true},
            {"numero_desvios", Integer, true},
            {"ate", Text, false},
            {"justificativa", Text, true},
            {"total_tempo_tex_executada", Text, false},
            {"parciais", Text, true},
            {"situacao_da_parcial", Text, true},
            {"ate_1", Text, false},
            {"ate_2", Text, false},
            {"desde_2", Text, false},
            {"total_tempo_tpo_executada", Text, false},
            {"atividade_especial", Text, true},
            {"equipamento_retirado", Text, true},
            {"sn_retirado", Text, false},
            {"destino", Text, true},
            {"equipamento_instalado", Text, true},
            {"sn_instalado", Text, false},
            {"sn_extra", Text, false},
            {"origem", Text, true},
            {"desativacao_da_localizacao", Text, true},
            {"instalacao_estimada", Text, true},
            {"executado", Text, true},
            {"concluido", Text, true},
            {"data_inicio_programada", DateText, false},
            {"data_programacao", DateText, false},
            {"data_inicio_reprogramada", DateText, false},
            {"data_reprogramacao", DateText, false},
            {"situacao_reprogramacao", Text, true},
            {"total_de_reprogramacoes", Integer, false},
            {"situacao_de_desvio", Text, true},
            {"numero_ssa_relacionada_1", Text, true},
            {"numero_ssa_relacionada_2", Text, true},
            {"numero_ssa_relacionada_3", Text, true},
            {"setor_emissor_relacionado_1", Text, true},
            {"setor_emissor_relacionado_2", Text, true},
            {"setor_executor_relacionado_1", Text, true},
            {"setor_executor_relacionado_2", Text, true},
            {"situacao_relacionada_1", Text, true},
            {"situacao_relacionada_2", Text, true},
            {"relacao", Text, true},
            {"data_arquivo_origem", DateText, false},
        }};

        template <typename Predicate> std::vector<std::string> keysMatching(Predicate predicate) {
            std::vector<std::string> result;
            result.reserve(kColumns.size());
            for (const auto& column : kColumns) {
                if (predicate(column)) {
                    result.push_back(column.key);
                }
            }
            return result;
        }

        const std::unordered_map<std::string_view, ColumnDef>& columnByKey() {
            static const std::unordered_map<std::string_view, ColumnDef> columns = [] {
                std::unordered_map<std::string_view, ColumnDef> result;
                result.reserve(kColumns.size());
                for (const auto& column : kColumns) {
                    if (!ColumnCatalog::isCanonicalStorageKey(column.key)) {
                        throw std::logic_error("column catalog contains a non-canonical key");
                    }
                    if (!result.emplace(column.key, column).second) {
                        throw std::logic_error("column catalog contains a duplicate key");
                    }
                }
                return result;
            }();
            return columns;
        }

        std::vector<std::string> buildOrderedFilterColumnKeys() {
            auto keys = keysMatching([](const ColumnDef& column) {
                return column.key != "id" && !ColumnCatalog::isDerivedCountColumn(column.key);
            });
            const auto status = std::string{ColumnCatalog::statusColumnKey()};
            const auto statusIt = std::ranges::find(keys, status);
            if (statusIt != keys.end() && statusIt != keys.begin()) {
                keys.erase(statusIt);
                keys.insert(keys.begin(), status);
            }
            return keys;
        }

    } // namespace

    std::span<const ColumnDef> ColumnCatalog::all() {
        return kColumns;
    }

    std::vector<ColumnDef> ColumnCatalog::schemaColumns() {
        return storageColumns();
    }

    std::span<const std::string_view> ColumnCatalog::requiredSchemaColumns() {
        static constexpr std::array<std::string_view, 3> required{"numero_ssa", "descricao_ssa",
                                                                  "data_cadastro"};
        return required;
    }

    std::vector<ColumnDef> ColumnCatalog::storageColumns() {
        std::vector<ColumnDef> result;
        std::ranges::copy_if(kColumns, std::back_inserter(result), [](const ColumnDef& column) {
            return !ColumnCatalog::isDerivedCountColumn(column.key);
        });
        return result;
    }

    std::vector<std::string> ColumnCatalog::defaultVisibleKeys() {
        std::vector<std::string> result;
        result.reserve(kDefaultVisibleColumnKeys.size());
        std::ranges::transform(kDefaultVisibleColumnKeys, std::back_inserter(result),
                               [](const auto key) { return std::string{key}; });
        return result;
    }

    std::vector<std::string> ColumnCatalog::visibleKeysOrDefault(std::vector<std::string> keys) {
        if (keys.empty()) {
            keys = defaultVisibleKeys();
        }
        const auto invalidKey =
            std::ranges::find_if(keys, [](const std::string& key) { return !contains(key); });
        if (invalidKey != keys.end()) {
            throw std::invalid_argument("unknown column: " + *invalidKey);
        }
        return keys;
    }

    std::vector<std::string> ColumnCatalog::generalSearchKeys() {
        return keysMatching([](const ColumnDef& column) { return column.generalSearch; });
    }

    const std::vector<std::string>& ColumnCatalog::orderedFilterColumnKeys() {
        static const auto keys = buildOrderedFilterColumnKeys();
        return keys;
    }

    std::span<const std::string_view> ColumnCatalog::advancedFilterKeys() {
        return kAdvancedFilterKeys;
    }

    std::string ColumnCatalog::defaultFilterColumnKey() {
        return "situacao";
    }

    std::string_view ColumnCatalog::statusColumnKey() {
        return kStatusColumnKey;
    }

    std::string_view ColumnCatalog::executorColumnKey() {
        return kExecutorColumnKey;
    }

    std::string_view ColumnCatalog::derivationColumnKey() {
        return kDerivationColumnKey;
    }

    std::string_view ColumnCatalog::derivedCountColumnKey() {
        return kDerivedCountColumnKey;
    }

    std::span<const std::string_view> ColumnCatalog::statusShortcutCodes() {
        return kStatusShortcutCodes;
    }

    std::string_view ColumnCatalog::downloadableStatusFilterExpression() {
        return kDownloadableStatusFilterExpression;
    }

    std::span<const std::string_view> ColumnCatalog::excludedStatusCodes() {
        return kExcludedStatusCodes;
    }

    bool ColumnCatalog::containsExcludedStatusCode(const std::string_view filterExpression) {
        const auto tokens = parseTextFilterTokens(filterExpression);
        return std::ranges::any_of(tokens.ordered, [](const auto& token) {
            return token.filterOperator == TextFilterOperator::Equals &&
                   std::ranges::find(kExcludedStatusCodes, std::string_view{token.value}) !=
                       kExcludedStatusCodes.end();
        });
    }

    std::span<const std::string_view> ColumnCatalog::weekColumnKeys() {
        return kWeekColumnKeys;
    }

    std::string_view ColumnCatalog::defaultAdvancedWeekColumnKey() {
        return kWeekColumnKeys[1];
    }

    std::string_view ColumnCatalog::issueWeekColumnKey() {
        return kWeekColumnKeys[0];
    }

    std::string_view ColumnCatalog::executionWeekColumnKey() {
        return kWeekColumnKeys[2];
    }

    std::span<const std::string_view> ColumnCatalog::reprogrammingColumnKeys() {
        return kReprogrammingColumnKeys;
    }

    std::string_view ColumnCatalog::primaryReprogrammingColumnKey() {
        return kReprogrammingColumnKeys.front();
    }

    std::string_view ColumnCatalog::statusLastSortCode() {
        return kStatusLastSortCode;
    }

    bool ColumnCatalog::isQuickSectorFilterColumn(const std::string_view key) {
        return key == kExecutorColumnKey;
    }

    bool ColumnCatalog::isStatusExclusionFilterColumn(const std::string_view key) {
        return key == kStatusColumnKey;
    }

    bool ColumnCatalog::isReprogrammingColumn(const std::string_view key) {
        return std::ranges::find(kReprogrammingColumnKeys, key) != kReprogrammingColumnKeys.end();
    }

    bool ColumnCatalog::isDerivedCountColumn(const std::string_view key) {
        return key == kDerivedCountColumnKey;
    }

    bool ColumnCatalog::isCanonicalStorageKey(const std::string_view key) {
        if (key.empty() || key.front() < 'a' || key.front() > 'z') {
            return false;
        }
        return std::ranges::all_of(key, [](const char value) {
            return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '_';
        });
    }

    const ColumnDef* ColumnCatalog::find(const std::string_view key) {
        const auto columnEntry = columnByKey().find(key);
        if (columnEntry == columnByKey().end()) {
            return nullptr;
        }
        return &columnEntry->second;
    }

    bool ColumnCatalog::contains(const std::string_view key) {
        return columnByKey().contains(key);
    }

} // namespace ssa::domain
