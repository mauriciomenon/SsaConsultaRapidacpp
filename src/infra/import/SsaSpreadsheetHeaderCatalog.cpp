#include "infra/import/SsaSpreadsheetHeaderCatalog.h"

#include "domain/ColumnCatalog.h"

#include <QChar>
#include <QString>

#include <array>
#include <string_view>
#include <unordered_map>

namespace ssa::infra::importing {

    namespace {

        struct HeaderAlias {
            std::string_view header;
            std::string_view canonical;
        };

        constexpr std::size_t kSourceAliasCount = 184;
        constexpr std::array<HeaderAlias, 168> kNormalizedAliases{
            HeaderAlias{"no ssa", "numero_ssa"},
            HeaderAlias{"no ssa*", "numero_ssa"},
            HeaderAlias{"no ssa original", "numero_ssa"},
            HeaderAlias{"numero ssa", "numero_ssa"},
            HeaderAlias{"no da ssa", "numero_ssa"},
            HeaderAlias{"numero da ssa", "numero_ssa"},
            HeaderAlias{"ssa_number", "numero_ssa"},
            HeaderAlias{"ssa number", "numero_ssa"},
            HeaderAlias{"ssanumber", "numero_ssa"},
            HeaderAlias{"numero de ssa", "numero_ssa"},
            HeaderAlias{"numero de la ssa", "numero_ssa"},
            HeaderAlias{"situacao", "situacao"},
            HeaderAlias{"status", "situacao"},
            HeaderAlias{"situation", "situacao"},
            HeaderAlias{"situation_desc", "situacao"},
            HeaderAlias{"situationdesc", "situacao"},
            HeaderAlias{"situacion", "situacao"},
            HeaderAlias{"estado", "situacao"},
            HeaderAlias{"process_status", "situacao"},
            HeaderAlias{"derivada de", "derivada_de"},
            HeaderAlias{"loc.", "localizacao_codigo"},
            HeaderAlias{"localizacao", "localizacao_codigo"},
            HeaderAlias{"cod. localizacao", "localizacao_codigo"},
            HeaderAlias{"codigo localizacao", "localizacao_codigo"},
            HeaderAlias{"localization", "localizacao_codigo"},
            HeaderAlias{"localizacion", "localizacao_codigo"},
            HeaderAlias{"ubicacion", "localizacao_codigo"},
            HeaderAlias{"desc. loc.", "descricao_localizacao"},
            HeaderAlias{"descricao da localizacao", "descricao_localizacao"},
            HeaderAlias{"descricao localizacao", "descricao_localizacao"},
            HeaderAlias{"equip.", "equipamento"},
            HeaderAlias{"equipamento", "equipamento"},
            HeaderAlias{"sem. cadastro", "semana_cadastro"},
            HeaderAlias{"semana cadastro", "semana_cadastro"},
            HeaderAlias{"semana de cadastro", "semana_cadastro"},
            HeaderAlias{"year_week", "semana_cadastro"},
            HeaderAlias{"year week", "semana_cadastro"},
            HeaderAlias{"ano semana", "semana_cadastro"},
            HeaderAlias{"ano-semana", "semana_cadastro"},
            HeaderAlias{"emitida em", "data_cadastro"},
            HeaderAlias{"data de emissao", "data_cadastro"},
            HeaderAlias{"data cadastro", "data_cadastro"},
            HeaderAlias{"data/hora de cadastro", "data_cadastro"},
            HeaderAlias{"issue_datetime", "data_cadastro"},
            HeaderAlias{"emission_datetime", "data_cadastro"},
            HeaderAlias{"issue datetime", "data_cadastro"},
            HeaderAlias{"emission datetime", "data_cadastro"},
            HeaderAlias{"fecha emision", "data_cadastro"},
            HeaderAlias{"fecha de emision", "data_cadastro"},
            HeaderAlias{"descricao da ssa", "descricao_ssa"},
            HeaderAlias{"descricao", "descricao_ssa"},
            HeaderAlias{"description", "descricao_ssa"},
            HeaderAlias{"descripcion", "descricao_ssa"},
            HeaderAlias{"descripcion de la ssa", "descricao_ssa"},
            HeaderAlias{"emissor", "setor_emissor"},
            HeaderAlias{"setor emissor", "setor_emissor"},
            HeaderAlias{"emitter_sector", "setor_emissor"},
            HeaderAlias{"emitter sector", "setor_emissor"},
            HeaderAlias{"emittersector", "setor_emissor"},
            HeaderAlias{"emmitersector", "setor_emissor"},
            HeaderAlias{"sector emisor", "setor_emissor"},
            HeaderAlias{"executor", "setor_executor"},
            HeaderAlias{"setor executor", "setor_executor"},
            HeaderAlias{"executor_sector", "setor_executor"},
            HeaderAlias{"executor sector", "setor_executor"},
            HeaderAlias{"executorsector", "setor_executor"},
            HeaderAlias{"sector ejecutor", "setor_executor"},
            HeaderAlias{"solicitante", "solicitante"},
            HeaderAlias{"serv. origem", "servico_origem"},
            HeaderAlias{"servico de origem", "servico_origem"},
            HeaderAlias{"prior. emissao", "grau_prioridade_emissao"},
            HeaderAlias{"prioridade emissao", "grau_prioridade_emissao"},
            HeaderAlias{"grau prioridade emissao", "grau_prioridade_emissao"},
            HeaderAlias{"grau de prioridade emissao", "grau_prioridade_emissao"},
            HeaderAlias{"prior. planej.", "grau_prioridade_planejamento"},
            HeaderAlias{"prioridade planejamento", "grau_prioridade_planejamento"},
            HeaderAlias{"grau prioridade planejamento", "grau_prioridade_planejamento"},
            HeaderAlias{"grau de prioridade planejamento", "grau_prioridade_planejamento"},
            HeaderAlias{"exec. simples", "execucao_simples"},
            HeaderAlias{"execucao simples", "execucao_simples"},
            HeaderAlias{"resp. prog.", "responsavel_programacao"},
            HeaderAlias{"responsavel programacao", "responsavel_programacao"},
            HeaderAlias{"responsavel na programacao", "responsavel_programacao"},
            HeaderAlias{"sem. prog.", "semana_programada"},
            HeaderAlias{"semana programada", "semana_programada"},
            HeaderAlias{"resp. exec.", "responsavel_execucao"},
            HeaderAlias{"responsavel execucao", "responsavel_execucao"},
            HeaderAlias{"responsavel na execucao", "responsavel_execucao"},
            HeaderAlias{"descricao da execucao", "descricao_execucao"},
            HeaderAlias{"descricao execucao", "descricao_execucao"},
            HeaderAlias{"execution_description", "descricao_execucao"},
            HeaderAlias{"execution description", "descricao_execucao"},
            HeaderAlias{"descripcion de la ejecucion", "descricao_execucao"},
            HeaderAlias{"total tempo tex executada", "total_tempo_tex_executada"},
            HeaderAlias{"parciais", "parciais"},
            HeaderAlias{"situacao da parcial", "situacao_da_parcial"},
            HeaderAlias{"prazo limite", "prazo_limite"},
            HeaderAlias{"situacao do prazo", "status_execucao_prazo"},
            HeaderAlias{"status prazo", "status_execucao_prazo"},
            HeaderAlias{"tempo disp.", "tempo_disponivel"},
            HeaderAlias{"tempo disponivel", "tempo_disponivel"},
            HeaderAlias{"data limite", "data_limite"},
            HeaderAlias{"tempo excedido", "tempo_excedido"},
            HeaderAlias{"desde", "desde"},
            HeaderAlias{"tempo total", "tempo_total"},
            HeaderAlias{"desde (1)", "desde_1"},
            HeaderAlias{"tempo tpe plan.", "total_tempo_tpe_planejado"},
            HeaderAlias{"total tempo tpe planejado", "total_tempo_tpe_planejado"},
            HeaderAlias{"tempo tex plan.", "total_tempo_tex_planejado"},
            HeaderAlias{"total tempo tex planejado", "total_tempo_tex_planejado"},
            HeaderAlias{"tempo tpo plan.", "total_tempo_tpo_planejado"},
            HeaderAlias{"total tempo tpo planejado", "total_tempo_tpo_planejado"},
            HeaderAlias{"horas prog.", "total_horas_programadas"},
            HeaderAlias{"total horas programadas", "total_horas_programadas"},
            HeaderAlias{"total de horas programadas", "total_horas_programadas"},
            HeaderAlias{"total tempo tpe executada", "total_tempo_tpe_executada"},
            HeaderAlias{"sem. exec.", "semana_executada"},
            HeaderAlias{"semana executada", "semana_executada"},
            HeaderAlias{"no reprog.", "num_reprogramacoes"},
            HeaderAlias{"numero de reprogramacoes", "num_reprogramacoes"},
            HeaderAlias{"n\u00b0 de reprogramacoes", "num_reprogramacoes"},
            HeaderAlias{"reprogramacoes", "num_reprogramacoes"},
            HeaderAlias{"exec. parcial", "execucao_parcial"},
            HeaderAlias{"execucao parcial", "execucao_parcial"},
            HeaderAlias{"anomalia", "anomalia"},
            HeaderAlias{"sis. origem", "sistema_origem"},
            HeaderAlias{"sistema de origem", "sistema_origem"},
            HeaderAlias{"registros de espera", "registros_espera"},
            HeaderAlias{"n\u00b0 de reprobaciones", "num_reprobaciones"},
            HeaderAlias{"no de reprobaciones", "num_reprobaciones"},
            HeaderAlias{"situacao de espera", "situacao_espera"},
            HeaderAlias{"numero de desvios", "numero_desvios"},
            HeaderAlias{"no de desvios", "numero_desvios"},
            HeaderAlias{"desvio", "numero_desvios"},
            HeaderAlias{"ate", "ate"},
            HeaderAlias{"justificativa", "justificativa"},
            HeaderAlias{"justificativa sem apr", "justificativa"},
            HeaderAlias{"atividade especial", "atividade_especial"},
            HeaderAlias{"actividad especial", "atividade_especial"},
            HeaderAlias{"equipamento retirado", "equipamento_retirado"},
            HeaderAlias{"destino", "destino"},
            HeaderAlias{"equipamento instalado", "equipamento_instalado"},
            HeaderAlias{"origem", "origem"},
            HeaderAlias{"desativacao da localizacao", "desativacao_da_localizacao"},
            HeaderAlias{"instalacao estimada", "instalacao_estimada"},
            HeaderAlias{"executado", "executado"},
            HeaderAlias{"concluido", "concluido"},
            HeaderAlias{"total tempo tpo executada", "total_tempo_tpo_executada"},
            HeaderAlias{"data inicio programada", "data_inicio_programada"},
            HeaderAlias{"data de programacao", "data_programacao"},
            HeaderAlias{"data inicio reprogramada", "data_inicio_reprogramada"},
            HeaderAlias{"data de reprogramacao", "data_reprogramacao"},
            HeaderAlias{"situacao de reprogramacao", "situacao_reprogramacao"},
            HeaderAlias{"total de reprogramacoes", "total_de_reprogramacoes"},
            HeaderAlias{"situacao de desvio", "situacao_de_desvio"},
            HeaderAlias{"relacao", "relacao"},
            HeaderAlias{"sn", "sn"},
            HeaderAlias{"ate (1)", "ate_1"},
            HeaderAlias{"ate (2)", "ate_2"},
            HeaderAlias{"desde (2)", "desde_2"},
            HeaderAlias{"numero da ssa relacionada", "numero_ssa_relacionada_1"},
            HeaderAlias{"numero da ssa relacionada (2)", "numero_ssa_relacionada_2"},
            HeaderAlias{"setor emissor relacionado", "setor_emissor_relacionado_1"},
            HeaderAlias{"setor emissor relacionado (2)", "setor_emissor_relacionado_2"},
            HeaderAlias{"setor executor relacionado", "setor_executor_relacionado_1"},
            HeaderAlias{"setor executor relacionado (2)", "setor_executor_relacionado_2"},
            HeaderAlias{"situacao relacionada", "situacao_relacionada_1"},
            HeaderAlias{"situacao relacionada (2)", "situacao_relacionada_2"},
        };

        std::string normalizedHeader(const std::string_view value) {
            const QString text =
                QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()))
                    .normalized(QString::NormalizationForm_KD);
            QString output;
            output.reserve(text.size());
            bool previousSpace = false;
            for (const auto ch : text) {
                if (ch.category() == QChar::Mark_NonSpacing) {
                    continue;
                }
                if (ch.isSpace()) {
                    if (!previousSpace) {
                        output.push_back(' ');
                    }
                    previousSpace = true;
                    continue;
                }
                output.push_back(ch.toLower());
                previousSpace = false;
            }
            return output.trimmed().toStdString();
        }

        const std::unordered_map<std::string, std::string>& headerAliases() {
            static const auto aliases = [] {
                std::unordered_map<std::string, std::string> values;
                values.reserve(kNormalizedAliases.size());
                for (const auto& alias : kNormalizedAliases) {
                    values.try_emplace(std::string{alias.header}, alias.canonical);
                }
                return values;
            }();
            return aliases;
        }

    } // namespace

    std::optional<std::string>
    SsaSpreadsheetHeaderCatalog::canonicalColumnForHeader(const std::string& header) {
        auto normalized = normalizedHeader(header);
        if (domain::ColumnCatalog::contains(normalized)) {
            return normalized;
        }
        const auto found = headerAliases().find(normalized);
        if (found != headerAliases().end() &&
            (found->second == "sn" || domain::ColumnCatalog::contains(found->second))) {
            return found->second;
        }
        return std::nullopt;
    }

    std::size_t SsaSpreadsheetHeaderCatalog::sourceLabelCount() {
        return kSourceAliasCount;
    }

} // namespace ssa::infra::importing
