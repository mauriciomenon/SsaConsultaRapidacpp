#include "presentation/DetailsViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaRelationGraph.h"

#include <QVariantMap>

namespace ssa::presentation {

    namespace {

        QString relationKindLabel(const domain::SsaRelationKind kind) {
            switch (kind) {
            case domain::SsaRelationKind::Current:
                return QStringLiteral("Atual");
            case domain::SsaRelationKind::DerivedFrom:
                return QStringLiteral("Derivada de");
            case domain::SsaRelationKind::Related:
                return QStringLiteral("Relacionada");
            }
            return {};
        }

        QVariantMap relationMap(const domain::SsaRelationItem& relation,
                                const std::string_view currentStatus) {
            QVariantMap item;
            item.insert(QStringLiteral("kind"), relationKindLabel(relation.kind));
            item.insert(QStringLiteral("ssa"), QString::fromStdString(relation.number));
            // Status (situacao) is only known for the Current node, which maps to
            // the displayed record. Related/Derived nodes require a repository
            // lookup that is out of scope for the inline details panel.
            if (relation.kind == domain::SsaRelationKind::Current && !currentStatus.empty()) {
                item.insert(QStringLiteral("status"),
                            QString::fromUtf8(currentStatus.data(),
                                              static_cast<qsizetype>(currentStatus.size())));
            }
            return item;
        }

        QVariantList buildRelations(const domain::SsaRecord& record) {
            const auto relationItems = domain::SsaRelationGraph::fromRecord(record);
            const auto currentStatus = record.valueOf(domain::ColumnCatalog::statusColumnKey());
            QVariantList relations;
            relations.reserve(static_cast<qsizetype>(relationItems.size()));
            for (const auto& relation : relationItems) {
                relations.append(relationMap(relation, currentStatus));
            }
            return relations;
        }

    } // namespace

    DetailsViewModel::DetailsViewModel(QObject* parent) : QObject(parent), fields_(this) {}

    QString DetailsViewModel::title() const {
        return title_;
    }

    DetailsFieldsModel* DetailsViewModel::fields() {
        return &fields_;
    }

    int DetailsViewModel::fieldCount() const {
        return fields_.rowCount();
    }

    void DetailsViewModel::setRecord(const domain::SsaRecord& record) {
        const auto selectedSsa = record.valueOf(domain::kSsaNumberColumnKey);
        selectedSsa_ =
            QString::fromUtf8(selectedSsa.data(), static_cast<qsizetype>(selectedSsa.size()));
        if (selectedSsa_.isEmpty()) {
            clearRecord();
            return;
        }
        title_ = "SSA " + selectedSsa_;
        relations_ = buildRelations(record);
        fields_.setRecord(record);
        emit changed();
    }

    void DetailsViewModel::clearRecord() {
        fields_.clear();
        selectedSsa_.clear();
        relations_.clear();
        title_ = "Nenhuma SSA selecionada";
        emit changed();
    }

    QString DetailsViewModel::selectedSsa() const {
        return selectedSsa_;
    }

    QString DetailsViewModel::selectedSsaNumber() const {
        return selectedSsa_;
    }

    QVariantList DetailsViewModel::relations() const {
        return relations_;
    }

    int DetailsViewModel::relationCount() const {
        return static_cast<int>(relations_.size());
    }

} // namespace ssa::presentation
