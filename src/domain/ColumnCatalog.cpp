#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <array>

namespace ssa::domain {

    namespace {

        using enum ColumnType;

        const std::array<ColumnDef, 84> kColumns{{
            {"id", "ID", Integer, false, false, 80},
            {"numero_ssa", "SSA", Text, true, true, 120},
            {"situacao", "Situacao", Text, true, true, 110},
            {"derivada_de", "Derivada de", Text, true, true, 120},
            {"localizacao_codigo", "Localizacao", Text, true, true, 140},
            {"descricao_localizacao", "Desc. Localizacao", Text, true, true, 220},
            {"equipamento", "Equipamento", Text, true, true, 150},
            {"semana_cadastro", "Sem. Cadastro", Integer, true, true, 120},
            {"data_cadastro", "Data Cadastro", DateText, true, false, 135},
            {"descricao_ssa", "Descricao SSA", Text, true, true, 360},
            {"descricao_execucao", "Descricao Execucao", Text, true, true, 360},
            {"setor_emissor", "Setor Emissor", Text, true, true, 140},
            {"setor_executor", "Setor Executor", Text, true, true, 140},
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
            {"semana_programada", "Sem. Programada", Integer, true, true, 140},
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
            {"semana_executada", "Sem. Executada", Integer, true, true, 140},
            {"num_reprogramacoes", "Reprogramacoes", Integer, false, false, 140},
            {"execucao_parcial", "Exec. Parcial", Text, false, true, 140},
            {"anomalia", "Anomalia", Text, false, true, 130},
            {"registros_espera", "Reg. Espera", Text, false, false, 140},
            {"num_reprobaciones", "Reprovacoes", Integer, false, true, 130},
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
        std::vector<std::string> result;
        for (const auto& column : kColumns) {
            if (column.defaultVisible) {
                result.push_back(column.key);
            }
        }
        return result;
    }

    std::vector<std::string> ColumnCatalog::generalSearchKeys() {
        std::vector<std::string> result;
        for (const auto& column : kColumns) {
            if (column.generalSearch) {
                result.push_back(column.key);
            }
        }
        return result;
    }

    std::optional<ColumnDef> ColumnCatalog::find(const std::string_view key) {
        const auto it = std::ranges::find_if(
            kColumns, [key](const ColumnDef& column) { return column.key == key; });
        if (it == kColumns.end()) {
            return std::nullopt;
        }
        return *it;
    }

    bool ColumnCatalog::contains(const std::string_view key) {
        return find(key).has_value();
    }

} // namespace ssa::domain
