#include "application/SsaColumnLabelCatalog.h"

#include <algorithm>
#include <array>
#include <ranges>

namespace ssa::application {

    namespace {

        constexpr std::array<SsaColumnLabel, 85> kLabels{{
            {"id", "ID", ""},
            {"numero_ssa", "No SSA", ""},
            {"situacao", "Sit.", "Situacao"},
            {"localizacao_codigo", "Loc.", "Localizacao"},
            {"setor_emissor", "Emis.", "Emissao"},
            {"setor_executor", "Exec.", "Executor"},
            {"qtd_derivadas", "Qtd Der.", "Qtd Derivadas"},
            {"descricao_ssa", "Descricao SSA", ""},
            {"data_cadastro", "Cadastro", ""},
            {"derivada_de", "Der. de", "Derivada da SSA:"},
            {"semana_cadastro", "Sem. Cad.", ""},
            {"descricao_localizacao", "Desc. Loc.", "Descricao Local"},
            {"equipamento", "Equip.", "Equipamento"},
            {"descricao_execucao", "Descricao Execucao", ""},
            {"solicitante", "Solicitante", ""},
            {"responsavel_programacao", "Resp. Programacao", ""},
            {"responsavel_execucao", "Resp. Execucao", ""},
            {"servico_origem", "Servico Origem", ""},
            {"sistema_origem", "Sistema Origem", ""},
            {"arquivo_origem", "Arquivo Origem", ""},
            {"data_planilha", "Data Planilha", ""},
            {"grau_prioridade_emissao", "Prior. Emissao", ""},
            {"grau_prioridade_planejamento", "Prior. Planej.", ""},
            {"execucao_simples", "Exec. Simples", ""},
            {"semana_programada", "Sem. Prog.", ""},
            {"prazo_limite", "Prazo Limite", ""},
            {"status_execucao_prazo", "Status Prazo", ""},
            {"tempo_disponivel", "Tempo Disponivel", ""},
            {"data_limite", "Data Limite", ""},
            {"tempo_excedido", "Tempo Excedido", ""},
            {"desde", "Desde", ""},
            {"tempo_total", "Tempo Total", ""},
            {"desde_1", "Desde 1", ""},
            {"total_tempo_tpe_planejado", "TPE Planejado", ""},
            {"total_tempo_tex_planejado", "TEX Planejado", ""},
            {"total_tempo_tpo_planejado", "TPO Planejado", ""},
            {"total_horas_programadas", "Horas Programadas", ""},
            {"total_tempo_tpe_executada", "TPE Executada", ""},
            {"semana_executada", "Sem. Exec.", ""},
            {"num_reprogramacoes", "Reprogramacoes", ""},
            {"execucao_parcial", "Exec. Parcial", ""},
            {"anomalia", "Anomalia", ""},
            {"registros_espera", "Reg. Espera", ""},
            {"num_reprobaciones", "Reprobaciones", ""},
            {"situacao_espera", "Situacao Espera", ""},
            {"numero_desvios", "Desvios", ""},
            {"ate", "Ate", ""},
            {"justificativa", "Justificativa", ""},
            {"total_tempo_tex_executada", "TEX Executada", ""},
            {"parciais", "Parciais", ""},
            {"situacao_da_parcial", "Situacao Parcial", ""},
            {"ate_1", "Ate 1", ""},
            {"ate_2", "Ate 2", ""},
            {"desde_2", "Desde 2", ""},
            {"total_tempo_tpo_executada", "TPO Executada", ""},
            {"atividade_especial", "Atividade Especial", ""},
            {"equipamento_retirado", "Equip. Retirado", ""},
            {"sn_retirado", "SN Retirado", ""},
            {"destino", "Destino", ""},
            {"equipamento_instalado", "Equip. Instalado", ""},
            {"sn_instalado", "SN Instalado", ""},
            {"sn_extra", "SN Extra", ""},
            {"origem", "Origem", ""},
            {"desativacao_da_localizacao", "Desativacao Local", ""},
            {"instalacao_estimada", "Instalacao Estimada", ""},
            {"executado", "Executado", ""},
            {"concluido", "Concluido", ""},
            {"data_inicio_programada", "Inicio Programado", ""},
            {"data_programacao", "Data Programacao", ""},
            {"data_inicio_reprogramada", "Inicio Reprogramado", ""},
            {"data_reprogramacao", "Data Reprogramacao", ""},
            {"situacao_reprogramacao", "Situacao Reprog.", ""},
            {"total_de_reprogramacoes", "Total Reprog.", ""},
            {"situacao_de_desvio", "Situacao Desvio", ""},
            {"numero_ssa_relacionada_1", "SSA Rel. 1", ""},
            {"numero_ssa_relacionada_2", "SSA Rel. 2", ""},
            {"numero_ssa_relacionada_3", "SSA Rel. 3", ""},
            {"setor_emissor_relacionado_1", "Setor Em. Rel. 1", ""},
            {"setor_emissor_relacionado_2", "Setor Em. Rel. 2", ""},
            {"setor_executor_relacionado_1", "Setor Ex. Rel. 1", ""},
            {"setor_executor_relacionado_2", "Setor Ex. Rel. 2", ""},
            {"situacao_relacionada_1", "Situacao Rel. 1", ""},
            {"situacao_relacionada_2", "Situacao Rel. 2", ""},
            {"relacao", "Relacao", ""},
            {"data_arquivo_origem", "Data Arq. Origem", ""},
        }};

    } // namespace

    std::span<const SsaColumnLabel> SsaColumnLabelCatalog::all() noexcept {
        return kLabels;
    }

    const SsaColumnLabel* SsaColumnLabelCatalog::find(const std::string_view key) noexcept {
        const auto label = std::ranges::find(kLabels, key, &SsaColumnLabel::key);
        return label == kLabels.end() ? nullptr : &*label;
    }

} // namespace ssa::application
