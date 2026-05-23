#include "infra/import/SsaSpreadsheetHeaderCatalog.h"

#include "domain/ColumnCatalog.h"

#include <unordered_map>

namespace ssa::infra::importing {

    namespace {

        const std::unordered_map<std::string, std::string>& headerAliases() {
            static const std::unordered_map<std::string, std::string> aliases{
                {"numero_ssa", "numero_ssa"},
                {"no ssa", "numero_ssa"},
                {"n ssa", "numero_ssa"},
                {"numero da ssa", "numero_ssa"},
                {"numero ssa", "numero_ssa"},
                {"situacao", "situacao"},
                {"status", "situacao"},
                {"derivada de", "derivada_de"},
                {"loc.", "localizacao_codigo"},
                {"localizacao", "localizacao_codigo"},
                {"cod. localizacao", "localizacao_codigo"},
                {"codigo localizacao", "localizacao_codigo"},
                {"desc. loc.", "descricao_localizacao"},
                {"descricao da localizacao", "descricao_localizacao"},
                {"descricao localizacao", "descricao_localizacao"},
                {"equip.", "equipamento"},
                {"equipamento", "equipamento"},
                {"sem. cadastro", "semana_cadastro"},
                {"sem. cad.", "semana_cadastro"},
                {"semana cadastro", "semana_cadastro"},
                {"semana de cadastro", "semana_cadastro"},
                {"emitida em", "data_cadastro"},
                {"data de emissao", "data_cadastro"},
                {"data cadastro", "data_cadastro"},
                {"data/hora de cadastro", "data_cadastro"},
                {"descricao da ssa", "descricao_ssa"},
                {"descricao ssa", "descricao_ssa"},
                {"descricao", "descricao_ssa"},
                {"descricao execucao", "descricao_execucao"},
                {"descricao da execucao", "descricao_execucao"},
                {"setor emissor", "setor_emissor"},
                {"emissor", "setor_emissor"},
                {"setor executor", "setor_executor"},
                {"executor", "setor_executor"},
                {"solicitante", "solicitante"},
                {"serv. origem", "servico_origem"},
                {"servico de origem", "servico_origem"},
                {"sis. origem", "sistema_origem"},
                {"sistema de origem", "sistema_origem"},
                {"prior. emissao", "grau_prioridade_emissao"},
                {"prioridade emissao", "grau_prioridade_emissao"},
                {"grau de prioridade emissao", "grau_prioridade_emissao"},
                {"prior. planej.", "grau_prioridade_planejamento"},
                {"prioridade planejamento", "grau_prioridade_planejamento"},
                {"grau de prioridade planejamento", "grau_prioridade_planejamento"},
                {"exec. simples", "execucao_simples"},
                {"execucao simples", "execucao_simples"},
                {"resp. prog.", "responsavel_programacao"},
                {"responsavel programacao", "responsavel_programacao"},
                {"responsavel na programacao", "responsavel_programacao"},
                {"sem. prog.", "semana_programada"},
                {"semana programada", "semana_programada"},
                {"resp. exec.", "responsavel_execucao"},
                {"responsavel execucao", "responsavel_execucao"},
                {"responsavel na execucao", "responsavel_execucao"},
                {"sem. exec.", "semana_executada"},
                {"semana executada", "semana_executada"},
                {"no reprog.", "num_reprogramacoes"},
                {"numero de reprogramacoes", "num_reprogramacoes"},
                {"reprogramacoes", "num_reprogramacoes"},
                {"total de reprogramacoes", "total_de_reprogramacoes"},
                {"anomalia", "anomalia"},
                {"justificativa", "justificativa"},
                {"relacao", "relacao"},
                {"numero da ssa relacionada", "numero_ssa_relacionada_1"},
                {"numero da ssa relacionada (2)", "numero_ssa_relacionada_2"},
                {"situacao relacionada", "situacao_relacionada_1"},
                {"situacao relacionada (2)", "situacao_relacionada_2"},
            };
            return aliases;
        }

    } // namespace

    std::optional<std::string>
    SsaSpreadsheetHeaderCatalog::canonicalColumnForHeader(const std::string& normalizedHeader) {
        if (domain::ColumnCatalog::contains(normalizedHeader)) {
            return normalizedHeader;
        }
        const auto found = headerAliases().find(normalizedHeader);
        if (found != headerAliases().end() && domain::ColumnCatalog::contains(found->second)) {
            return found->second;
        }
        return std::nullopt;
    }

} // namespace ssa::infra::importing
