#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_map>

namespace ssa::domain {

    namespace {

        using enum ColumnType;

        constexpr std::string_view kStatusColumnKey = "situacao";
        constexpr std::string_view kExecutorColumnKey = "setor_executor";
        constexpr std::string_view kDerivationColumnKey = "derivada_de";
        constexpr std::array<std::string_view, 3> kExcludedStatusCodes{"SCA", "SES", "STE"};
        constexpr std::array<std::string_view, 3> kWeekColumnKeys{
            "semana_cadastro", "semana_programada", "semana_executada"};
        constexpr std::array<std::string_view, 2> kReprogrammingColumnKeys{
            "num_reprogramacoes", "total_de_reprogramacoes"};
        struct AdvancedFilterColumnDef {
            std::string_view key;
            std::string_view label;
        };

        constexpr std::array<AdvancedFilterColumnDef, 8> kAdvancedFilterColumns{{
            {"setor_emissor", "Setor emissor"},
            {"setor_executor", "Setor executor"},
            {"situacao", "Situacao"},
            {"grau_prioridade_emissao", "Prioridade emissao"},
            {"grau_prioridade_planejamento", "Prioridade planejamento"},
            {"solicitante", "Solicitante"},
            {"responsavel_programacao", "Responsavel programacao"},
            {"responsavel_execucao", "Responsavel execucao"},
        }};
        constexpr std::string_view kStatusLastSortCode = "STE";

        const std::array<ColumnDef, 84> kColumns{{
            {"id", "ID", Integer, false, false, 80},
            {"numero_ssa", "No SSA", Text, true, true, 110},
            {"situacao", "Sit.", Text, true, true, 80},
            {"derivada_de", "Der.", Text, true, true, 110},
            {"localizacao_codigo", "Loc.", Text, true, true, 120},
            {"descricao_localizacao", "Desc. Loc.", Text, true, true, 220},
            {"equipamento", "Equip.", Text, true, true, 150},
            {"semana_cadastro", "Sem. Cad.", Integer, true, true, 95},
            {"data_cadastro", "Cadastro", DateText, true, false, 120},
            {"descricao_ssa", "Descricao SSA", Text, true, true, 360},
            {"descricao_execucao", "Descricao Execucao", Text, true, true, 360},
            {"setor_emissor", "Emis.", Text, true, true, 90},
            {"setor_executor", "Exec.", Text, true, true, 90},
            {"solicitante", "Solicitante", Text, true, true, 180},
            {"responsavel_programacao", "Resp. Programacao", Text, true, true, 180},
            {"responsavel_execucao", "Resp. Execucao", Text, true, true, 180},
            {"servico_origem", "Servico Origem", Text, false, true, 150},
            {"sistema_origem", "Sistema Origem", Text, false, true, 150},
            {"arquivo_origem", "Arquivo Origem", Text, false, false, 180},
            {"data_planilha", "Data Planilha", DateText, false, false, 130},
            {"grau_prioridade_emissao", "Prior. Emissao", Text, true, true, 140},
            {"grau_prioridade_planejamento", "Prior. Planej.", Text, true, true, 140},
            {"execucao_simples", "Exec. Simples", Text, false, false, 130},
            {"semana_programada", "Sem. Prog.", Integer, true, true, 95},
            {"prazo_limite", "Prazo Limite", DateText, false, false, 140},
            {"status_execucao_prazo", "Status Prazo", Text, false, false, 150},
            {"tempo_disponivel", "Tempo Disponivel", Text, false, false, 150},
            {"data_limite", "Data Limite", DateText, false, false, 130},
            {"tempo_excedido", "Tempo Excedido", Text, false, false, 150},
            {"desde", "Desde", Text, false, false, 120},
            {"tempo_total", "Tempo Total", Text, false, false, 130},
            {"desde_1", "Desde 1", Text, false, false, 120},
            {"total_tempo_tpe_planejado", "TPE Planejado", Text, false, false, 150},
            {"total_tempo_tex_planejado", "TEX Planejado", Text, false, false, 150},
            {"total_tempo_tpo_planejado", "TPO Planejado", Text, false, false, 150},
            {"total_horas_programadas", "Horas Programadas", Text, false, false, 160},
            {"total_tempo_tpe_executada", "TPE Executada", Text, false, false, 150},
            {"semana_executada", "Sem. Exec.", Integer, true, true, 95},
            {"num_reprogramacoes", "Reprogramacoes", Integer, false, false, 140},
            {"execucao_parcial", "Exec. Parcial", Text, false, true, 140},
            {"anomalia", "Anomalia", Text, false, true, 130},
            {"registros_espera", "Reg. Espera", Text, false, false, 140},
            {"num_reprobaciones", "Reprobaciones", Integer, false, false, 130},
            {"situacao_espera", "Situacao Espera", Text, false, true, 150},
            {"numero_desvios", "Desvios", Integer, false, true, 120},
            {"ate", "Ate", Text, false, false, 120},
            {"justificativa", "Justificativa", Text, false, true, 260},
            {"total_tempo_tex_executada", "TEX Executada", Text, false, false, 150},
            {"parciais", "Parciais", Text, false, true, 150},
            {"situacao_da_parcial", "Situacao Parcial", Text, false, true, 160},
            {"ate_1", "Ate 1", Text, false, false, 120},
            {"ate_2", "Ate 2", Text, false, false, 120},
            {"desde_2", "Desde 2", Text, false, false, 120},
            {"total_tempo_tpo_executada", "TPO Executada", Text, false, false, 150},
            {"atividade_especial", "Atividade Especial", Text, false, true, 170},
            {"equipamento_retirado", "Equip. Retirado", Text, false, true, 170},
            {"sn_retirado", "SN Retirado", Text, false, false, 140},
            {"destino", "Destino", Text, false, true, 150},
            {"equipamento_instalado", "Equip. Instalado", Text, false, true, 170},
            {"sn_instalado", "SN Instalado", Text, false, false, 140},
            {"sn_extra", "SN Extra", Text, false, false, 130},
            {"origem", "Origem", Text, false, true, 150},
            {"desativacao_da_localizacao", "Desativacao Local", Text, false, true, 180},
            {"instalacao_estimada", "Instalacao Estimada", Text, false, true, 180},
            {"executado", "Executado", Text, false, true, 130},
            {"concluido", "Concluido", Text, false, true, 130},
            {"data_inicio_programada", "Inicio Programado", DateText, false, false, 160},
            {"data_programacao", "Data Programacao", DateText, false, false, 160},
            {"data_inicio_reprogramada", "Inicio Reprogramado", DateText, false, false, 170},
            {"data_reprogramacao", "Data Reprogramacao", DateText, false, false, 170},
            {"situacao_reprogramacao", "Situacao Reprog.", Text, false, true, 170},
            {"total_de_reprogramacoes", "Total Reprog.", Integer, false, false, 140},
            {"situacao_de_desvio", "Situacao Desvio", Text, false, true, 160},
            {"numero_ssa_relacionada_1", "SSA Rel. 1", Text, false, true, 130},
            {"numero_ssa_relacionada_2", "SSA Rel. 2", Text, false, true, 130},
            {"numero_ssa_relacionada_3", "SSA Rel. 3", Text, false, true, 130},
            {"setor_emissor_relacionado_1", "Setor Em. Rel. 1", Text, false, true, 160},
            {"setor_emissor_relacionado_2", "Setor Em. Rel. 2", Text, false, true, 160},
            {"setor_executor_relacionado_1", "Setor Ex. Rel. 1", Text, false, true, 160},
            {"setor_executor_relacionado_2", "Setor Ex. Rel. 2", Text, false, true, 160},
            {"situacao_relacionada_1", "Situacao Rel. 1", Text, false, true, 160},
            {"situacao_relacionada_2", "Situacao Rel. 2", Text, false, true, 160},
            {"relacao", "Relacao", Text, false, true, 160},
            {"data_arquivo_origem", "Data Arq. Origem", DateText, false, false, 160},
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
                    result.emplace(column.key, column);
                }
                return result;
            }();
            return columns;
        }

        std::vector<std::string> buildOrderedFilterColumnKeys() {
            auto keys = keysMatching([](const ColumnDef& column) { return column.key != "id"; });
            const auto status = std::string{ColumnCatalog::statusColumnKey()};
            const auto statusIt = std::ranges::find(keys, status);
            if (statusIt != keys.end() && statusIt != keys.begin()) {
                keys.erase(statusIt);
                keys.insert(keys.begin(), status);
            }
            return keys;
        }

        constexpr std::array<std::string_view, kAdvancedFilterColumns.size()>
        buildAdvancedFilterKeys() {
            std::array<std::string_view, kAdvancedFilterColumns.size()> keys{};
            for (std::size_t index = 0; index < kAdvancedFilterColumns.size(); ++index) {
                keys[index] = kAdvancedFilterColumns[index].key;
            }
            return keys;
        }

    } // namespace

    std::span<const ColumnDef> ColumnCatalog::all() {
        return kColumns;
    }

    std::vector<ColumnDef> ColumnCatalog::defaultVisible() {
        std::vector<ColumnDef> result;
        std::ranges::copy_if(kColumns, std::back_inserter(result),
                             [](const ColumnDef& column) { return column.defaultVisible; });
        return result;
    }

    std::vector<std::string> ColumnCatalog::defaultVisibleKeys() {
        return keysMatching([](const ColumnDef& column) { return column.defaultVisible; });
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
        static constexpr auto kAdvancedFilterKeys = buildAdvancedFilterKeys();
        return kAdvancedFilterKeys;
    }

    std::string_view ColumnCatalog::advancedFilterLabel(const std::string_view key) {
        const auto advancedColumn = std::ranges::find_if(
            kAdvancedFilterColumns,
            [key](const AdvancedFilterColumnDef& column) { return column.key == key; });
        if (advancedColumn != kAdvancedFilterColumns.end()) {
            return advancedColumn->label;
        }
        if (const auto* column = find(key); column != nullptr) {
            return column->label;
        }
        return key;
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

    std::span<const std::string_view> ColumnCatalog::excludedStatusCodes() {
        return kExcludedStatusCodes;
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

    const ColumnDef* ColumnCatalog::find(const std::string_view key) {
        const auto it = columnByKey().find(key);
        if (it == columnByKey().end()) {
            return nullptr;
        }
        return &it->second;
    }

    bool ColumnCatalog::contains(const std::string_view key) {
        return columnByKey().contains(key);
    }

} // namespace ssa::domain
