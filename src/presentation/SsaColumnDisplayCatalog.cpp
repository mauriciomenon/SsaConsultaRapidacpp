#include "presentation/SsaColumnDisplayCatalog.h"

#include "application/SsaColumnLabelCatalog.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
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

        constexpr std::array<int, 85> kDefaultWidths{{
            80,  98,  60,  84,  68,  68,  66,  640, 100, 88,  84,  220, 150, 360, 240, 250, 250,
            150, 150, 180, 130, 140, 140, 130, 86,  140, 150, 150, 130, 150, 120, 130, 120, 150,
            150, 150, 160, 150, 86,  140, 140, 130, 140, 130, 150, 120, 120, 260, 150, 150, 160,
            120, 120, 120, 150, 170, 170, 140, 150, 170, 140, 130, 150, 180, 180, 130, 130, 160,
            160, 170, 170, 170, 140, 160, 130, 130, 130, 160, 160, 160, 160, 160, 160, 160, 160,
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
        const auto index = static_cast<std::size_t>(std::distance(columns.begin(), column));
        if (index >= kDefaultWidths.size()) {
            throw std::logic_error("display width catalog does not match domain columns");
        }
        const auto* labels = application::SsaColumnLabelCatalog::find(key);
        if (labels == nullptr) {
            throw std::logic_error("display label catalog does not match domain columns");
        }
        return {column->key,  std::string{labels->label},    std::string{labels->labelFull},
                column->type, isDefaultVisible(column->key), kDefaultWidths[index]};
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
