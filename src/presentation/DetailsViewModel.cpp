#include "presentation/DetailsViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaRelationGraph.h"
#include "query/SsaQueryService.h"

#include <QVariantMap>

#include <utility>

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
                                const std::string_view status) {
            QVariantMap item;
            item.insert(QStringLiteral("kind"), relationKindLabel(relation.kind));
            item.insert(QStringLiteral("ssa"), QString::fromStdString(relation.number));
            if (!status.empty()) {
                item.insert(
                    QStringLiteral("status"),
                    QString::fromUtf8(status.data(), static_cast<qsizetype>(status.size())));
            }
            return item;
        }

        // Local relations (ancestral derivada_de + 3 relacionados) read from the
        // record fields. The Current node carries its own situacao.
        QVariantList buildLocalRelations(const domain::SsaRecord& record) {
            const auto relationItems = domain::SsaRelationGraph::fromRecord(record);
            const auto currentStatus = record.valueOf(domain::ColumnCatalog::statusColumnKey());
            QVariantList relations;
            relations.reserve(static_cast<qsizetype>(relationItems.size()));
            for (const auto& relation : relationItems) {
                const auto status = relation.kind == domain::SsaRelationKind::Current
                                        ? currentStatus
                                        : std::string_view{};
                relations.append(relationMap(relation, status));
            }
            return relations;
        }

        // Direct children (filhas) fetched from the repository: one level only.
        void appendDirectChildren(QVariantList& relations,
                                  const std::vector<domain::SsaDerivadaEntry>& children) {
            for (const auto& child : children) {
                QVariantMap item;
                item.insert(QStringLiteral("kind"), QStringLiteral("Derivada"));
                item.insert(QStringLiteral("ssa"), QString::fromStdString(child.number));
                if (!child.situacao.empty()) {
                    item.insert(QStringLiteral("status"), QString::fromStdString(child.situacao));
                }
                relations.append(item);
            }
        }

    } // namespace

    DetailsViewModel::DetailsViewModel(QObject* parent) : QObject(parent), fields_(this) {}

    DetailsViewModel::DetailsViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                       QObject* parent)
        : QObject(parent), fields_(this), queryService_(std::move(queryService)) {}

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
        rebuildDerivadas(record);
        fields_.setRecord(record);
        emit changed();
    }

    void DetailsViewModel::clearRecord() {
        fields_.clear();
        selectedSsa_.clear();
        relations_.clear();
        graphModel_.buildFromRelations({}, {});
        title_ = "Nenhuma SSA selecionada";
        emit changed();
    }

    DerivadasGraphModel* DetailsViewModel::graphModel() {
        return &graphModel_;
    }

    void DetailsViewModel::rebuildDerivadas(const domain::SsaRecord& record) {
        relations_ = buildLocalRelations(record);
        // Query direct children (one level) when a repository is wired.
        if (queryService_ && !selectedSsa_.isEmpty()) {
            const auto children =
                queryService_->derivadasDiretas(domain::SsaNumber{selectedSsa_.toStdString()});
            appendDirectChildren(relations_, children);
        }
        graphModel_.buildFromRelations(selectedSsa_, relations_);
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
