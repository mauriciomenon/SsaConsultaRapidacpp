#include "domain/SsaRelationGraph.h"

#include <array>

namespace ssa::domain {

    namespace {

        constexpr std::string_view kDerivedFromKey = "derivada_de";
        constexpr std::array<std::string_view, 3> kRelatedSsaKeys{
            "numero_ssa_relacionada_1",
            "numero_ssa_relacionada_2",
            "numero_ssa_relacionada_3",
        };
        void appendRelation(std::vector<SsaRelationItem>& relations, const SsaRelationKind kind,
                            const std::string_view number) {
            if (number.empty()) {
                return;
            }
            relations.push_back({kind, std::string{number}});
        }

    } // namespace

    std::vector<SsaRelationItem> SsaRelationGraph::fromRecord(const SsaRecord& record) {
        std::vector<SsaRelationItem> relations;
        relations.reserve(kRelatedSsaKeys.size() + 2);
        appendRelation(relations, SsaRelationKind::Current, record.valueOf(kSsaNumberColumnKey));
        appendRelation(relations, SsaRelationKind::DerivedFrom, record.valueOf(kDerivedFromKey));
        for (const auto key : kRelatedSsaKeys) {
            appendRelation(relations, SsaRelationKind::Related, record.valueOf(key));
        }
        return relations;
    }

} // namespace ssa::domain
