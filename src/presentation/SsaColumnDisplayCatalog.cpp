#include "presentation/SsaColumnDisplayCatalog.h"

#include <algorithm>
#include <array>

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

        const AdvancedFilterDisplay* advancedDisplay(const std::string_view key) {
            const auto entry =
                std::ranges::find(kAdvancedFilterDisplay, key, &AdvancedFilterDisplay::key);
            return entry == kAdvancedFilterDisplay.end() ? nullptr : &*entry;
        }

    } // namespace

    SsaDisplayColumn SsaColumnDisplayCatalog::resolve(const std::string& key) const {
        const auto column = domain::ColumnCatalog::find(key);
        if (!column) {
            return {key, key, {}, domain::ColumnType::Text, 132};
        }
        return {column->key, column->label, column->labelFull, column->type, column->defaultWidth};
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
