#include "presentation/DetailsViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaRelationGraph.h"
#include "query/SsaQueryService.h"

#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace ssa::presentation {

    namespace {

        QString relationKindLabel(const domain::SsaRelationKind kind) {
            switch (kind) {
            case domain::SsaRelationKind::Current:
                return QStringLiteral("");
            case domain::SsaRelationKind::DerivedFrom:
                return QStringLiteral("Der.");
            case domain::SsaRelationKind::Related:
                return QStringLiteral("Rel.");
            }
            return {};
        }

        QString relationRole(const domain::SsaRelationKind kind) {
            switch (kind) {
            case domain::SsaRelationKind::Current:
                return QStringLiteral("current");
            case domain::SsaRelationKind::DerivedFrom:
                return QStringLiteral("parent");
            case domain::SsaRelationKind::Related:
                return QStringLiteral("related");
            }
            return {};
        }

        QVariantMap relationMap(const domain::SsaRelationItem& relation,
                                const std::string_view status) {
            QVariantMap item;
            item.insert(QStringLiteral("kind"), relationKindLabel(relation.kind));
            item.insert(QStringLiteral("role"), relationRole(relation.kind));
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
                item.insert(QStringLiteral("kind"), QStringLiteral("Der."));
                item.insert(QStringLiteral("role"), QStringLiteral("child"));
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
        // Emit relationNavigationChanged after changed() so the QML sees the
        // updated relationCount before re-evaluating canSelect* flags.
        currentRelationIndex_ = 0;
        emit relationNavigationChanged();
    }

    void DetailsViewModel::clearRecord() {
        fields_.clear();
        selectedSsa_.clear();
        relations_.clear();
        graphModel_.buildFromRelations({}, {});
        title_ = "Nenhuma SSA selecionada";
        setCurrentRelationIndex(0);
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

    bool DetailsViewModel::loadBySsaNumber(const QString& ssaNumber) {
        if (!queryService_) {
            return false;
        }
        const auto trimmed = ssaNumber.trimmed();
        if (trimmed.isEmpty()) {
            return false;
        }
        const auto record = queryService_->details(domain::SsaNumber{trimmed.toStdString()});
        if (!record) {
            return false;
        }
        setRecord(*record);
        return true;
    }

    int DetailsViewModel::currentRelationIndex() const {
        return currentRelationIndex_;
    }

    bool DetailsViewModel::canSelectNextRelation() const {
        return currentRelationIndex_ >= 0 && currentRelationIndex_ < relationCount() - 1;
    }

    bool DetailsViewModel::canSelectPreviousRelation() const {
        return currentRelationIndex_ > 0;
    }

    void DetailsViewModel::selectNextRelation() {
        if (!canSelectNextRelation()) {
            return;
        }
        loadRelation(currentRelationIndex_ + 1);
    }

    void DetailsViewModel::selectPreviousRelation() {
        if (!canSelectPreviousRelation()) {
            return;
        }
        loadRelation(currentRelationIndex_ - 1);
    }

    void DetailsViewModel::setCurrentRelationIndex(const int index) {
        if (currentRelationIndex_ == index) {
            return;
        }
        currentRelationIndex_ = index;
        emit relationNavigationChanged();
    }

    void DetailsViewModel::loadRelation(const int index) {
        if (index < 0 || index >= relations_.size()) {
            return;
        }
        const auto ssa = relations_.at(index).toMap().value(QStringLiteral("ssa")).toString();
        // Advance the navigation index regardless of whether the load succeeds:
        // the UI must reflect the user's position in the chain.
        setCurrentRelationIndex(index);
        if (!ssa.isEmpty() && ssa != selectedSsa_) {
            const bool loaded = loadBySsaNumber(ssa);
            if (!loaded) {
                return;
            }
            // loadBySsaNumber resets the index to 0 (Current node of the loaded
            // SSA). Keep the chain position only when it still exists.
            const int lastIndex = std::max(0, relationCount() - 1);
            setCurrentRelationIndex(std::min(index, lastIndex));
        }
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
