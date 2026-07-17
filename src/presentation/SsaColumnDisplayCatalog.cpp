#include "presentation/SsaColumnDisplayCatalog.h"

#include "application/SsaColumnLabelCatalog.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace ssa::presentation {

    namespace {

        struct AdvancedFilterDisplay final {
            std::string_view key;
            std::string_view label;
            std::string_view shortLabel;
        };

        constexpr std::array<AdvancedFilterDisplay, 11> kAdvancedFilterDisplay{{
            {"setor_emissor", "Setor emissor", "Emis."},
            {"setor_executor", "Setor executor", "Exec."},
            {"situacao", "Situacao", "Sit."},
            {"solicitante", "Solicitante", "Solicit."},
            {"responsavel_programacao", "Responsavel programacao", "Resp. Plan."},
            {"responsavel_execucao", "Responsavel execucao", "Resp. Exec."},
            {"localizacao_codigo", "Localizacao", "Loc."},
            {"status_execucao_prazo", "Status prazo", "Stat. Prazo"},
            {"anomalia", "Anomalia", "Anom."},
            {"situacao_espera", "Situacao espera", "Sit. Esp."},
            {"situacao_da_parcial", "Situacao parcial", "Sit. Parc."},
        }};

        struct DefaultWidth final {
            std::string_view key;
            int width{0};
        };

        constexpr std::array<DefaultWidth, 85> kDefaultWidths{{
            {"id", 80},
            {"numero_ssa", 98},
            {"situacao", 60},
            {"localizacao_codigo", 84},
            {"setor_emissor", 68},
            {"setor_executor", 68},
            {"qtd_derivadas", 66},
            {"descricao_ssa", 640},
            {"data_cadastro", 100},
            {"derivada_de", 88},
            {"semana_cadastro", 84},
            {"descricao_localizacao", 220},
            {"equipamento", 150},
            {"descricao_execucao", 360},
            {"solicitante", 240},
            {"responsavel_programacao", 250},
            {"responsavel_execucao", 250},
            {"servico_origem", 150},
            {"sistema_origem", 150},
            {"arquivo_origem", 180},
            {"data_planilha", 130},
            {"grau_prioridade_emissao", 140},
            {"grau_prioridade_planejamento", 140},
            {"execucao_simples", 130},
            {"semana_programada", 86},
            {"prazo_limite", 140},
            {"status_execucao_prazo", 150},
            {"tempo_disponivel", 150},
            {"data_limite", 130},
            {"tempo_excedido", 150},
            {"desde", 120},
            {"tempo_total", 130},
            {"desde_1", 120},
            {"total_tempo_tpe_planejado", 150},
            {"total_tempo_tex_planejado", 150},
            {"total_tempo_tpo_planejado", 150},
            {"total_horas_programadas", 160},
            {"total_tempo_tpe_executada", 150},
            {"semana_executada", 86},
            {"num_reprogramacoes", 140},
            {"execucao_parcial", 140},
            {"anomalia", 130},
            {"registros_espera", 140},
            {"num_reprobaciones", 130},
            {"situacao_espera", 150},
            {"numero_desvios", 120},
            {"ate", 120},
            {"justificativa", 260},
            {"total_tempo_tex_executada", 150},
            {"parciais", 150},
            {"situacao_da_parcial", 160},
            {"ate_1", 120},
            {"ate_2", 120},
            {"desde_2", 120},
            {"total_tempo_tpo_executada", 150},
            {"atividade_especial", 170},
            {"equipamento_retirado", 170},
            {"sn_retirado", 140},
            {"destino", 150},
            {"equipamento_instalado", 170},
            {"sn_instalado", 140},
            {"sn_extra", 130},
            {"origem", 150},
            {"desativacao_da_localizacao", 180},
            {"instalacao_estimada", 180},
            {"executado", 130},
            {"concluido", 130},
            {"data_inicio_programada", 160},
            {"data_programacao", 160},
            {"data_inicio_reprogramada", 170},
            {"data_reprogramacao", 170},
            {"situacao_reprogramacao", 170},
            {"total_de_reprogramacoes", 140},
            {"situacao_de_desvio", 160},
            {"numero_ssa_relacionada_1", 130},
            {"numero_ssa_relacionada_2", 130},
            {"numero_ssa_relacionada_3", 130},
            {"setor_emissor_relacionado_1", 160},
            {"setor_emissor_relacionado_2", 160},
            {"setor_executor_relacionado_1", 160},
            {"setor_executor_relacionado_2", 160},
            {"situacao_relacionada_1", 160},
            {"situacao_relacionada_2", 160},
            {"relacao", 160},
            {"data_arquivo_origem", 160},
        }};

        const AdvancedFilterDisplay* advancedDisplay(const std::string_view key) {
            const auto entry =
                std::ranges::find(kAdvancedFilterDisplay, key, &AdvancedFilterDisplay::key);
            return entry == kAdvancedFilterDisplay.end() ? nullptr : &*entry;
        }

        bool isDefaultVisible(const std::string_view key) {
            const auto keys = domain::ColumnCatalog::defaultVisibleKeys();
            return std::ranges::find(keys, key) != keys.end();
        }

    } // namespace

    SsaDisplayColumn SsaColumnDisplayCatalog::resolve(const std::string& key) const {
        const auto columns = domain::ColumnCatalog::all();
        const auto column = std::ranges::find(columns, key, &domain::ColumnDef::key);
        if (column == columns.end()) {
            throw std::invalid_argument("unknown display column: " + key);
        }
        const auto defaultWidth = std::ranges::find(kDefaultWidths, key, &DefaultWidth::key);
        if (defaultWidth == kDefaultWidths.end()) {
            throw std::logic_error("display width catalog does not match domain columns");
        }
        const auto* labels = application::SsaColumnLabelCatalog::find(key);
        if (labels == nullptr) {
            throw std::logic_error("display label catalog does not match domain columns");
        }
        return {column->key,  std::string{labels->label},    std::string{labels->labelFull},
                column->type, isDefaultVisible(column->key), defaultWidth->width};
    }

    std::vector<SsaDisplayColumn>
    SsaColumnDisplayCatalog::resolveAll(const std::vector<std::string>& keys) const {
        std::vector<SsaDisplayColumn> columns;
        columns.reserve(keys.size());
        for (const auto& key : keys) {
            columns.push_back(resolve(key));
        }
        return columns;
    }

    std::vector<SsaDisplayColumn> SsaColumnDisplayCatalog::all() const {
        std::vector<SsaDisplayColumn> columns;
        columns.reserve(domain::ColumnCatalog::all().size());
        for (const auto& column : domain::ColumnCatalog::all()) {
            columns.push_back(resolve(column.key));
        }
        return columns;
    }

    std::string SsaColumnDisplayCatalog::advancedFilterLabel(const std::string_view key) const {
        if (const auto* display = advancedDisplay(key); display != nullptr) {
            return std::string{display->label};
        }
        return resolve(std::string{key}).label;
    }

    std::string
    SsaColumnDisplayCatalog::advancedFilterShortLabel(const std::string_view key) const {
        if (const auto* display = advancedDisplay(key); display != nullptr) {
            return std::string{display->shortLabel};
        }
        return resolve(std::string{key}).label;
    }

} // namespace ssa::presentation
