#include "presentation/DetailsViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaRelationGraph.h"
#include "presentation/AsyncOperationErrorLog.h"
#include "query/SsaQueryService.h"

#include <QVariantMap>
#include <QtConcurrentRun>

#include <algorithm>
#include <system_error>
#include <utility>

namespace ssa::presentation {

    namespace {

        QString relationKindLabel(const domain::SsaRelationKind kind) {
            switch (kind) {
            case domain::SsaRelationKind::Current:
                return QStringLiteral("Atual");
            case domain::SsaRelationKind::DerivedFrom:
                return QStringLiteral("Origem");
            case domain::SsaRelationKind::Related:
                return QStringLiteral("Relacionada");
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

        int currentRelationIndexFor(const QVariantList& relations, const QString& selectedSsa) {
            for (qsizetype index = 0; index < relations.size(); ++index) {
                const auto map = relations.at(index).toMap();
                const auto role = map.value(QStringLiteral("role")).toString();
                const auto ssa = map.value(QStringLiteral("ssa")).toString();
                if (role == QStringLiteral("current") || ssa == selectedSsa) {
                    return static_cast<int>(index);
                }
            }
            return 0;
        }

        // Local relations (ancestral derivada_de + current + 3 relacionados)
        // read from the record fields. The Current node carries its own situacao.
        QVariantList buildLocalRelations(const domain::SsaRecord& record) {
            const auto relationItems = domain::SsaRelationGraph::fromRecord(record);
            const auto currentStatus = record.valueOf(domain::ColumnCatalog::statusColumnKey());
            QVariantList relations;
            relations.reserve(static_cast<qsizetype>(relationItems.size()));
            for (const auto& relation : relationItems) {
                if (relation.kind == domain::SsaRelationKind::DerivedFrom) {
                    relations.append(relationMap(relation, std::string_view{}));
                }
            }
            for (const auto& relation : relationItems) {
                if (relation.kind != domain::SsaRelationKind::Current) {
                    continue;
                }
                relations.append(relationMap(relation, currentStatus));
            }
            for (const auto& relation : relationItems) {
                if (relation.kind != domain::SsaRelationKind::Related) {
                    continue;
                }
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

    DetailsViewModel::~DetailsViewModel() {
        shuttingDown_ = true;
        for (const auto& operation : relationQueries_) {
            disconnect(&operation->watcher, nullptr, this, nullptr);
        }
        stopRelationQueries();
    }

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
        currentRelationIndex_ = currentRelationIndexFor(relations_, selectedSsa_);
        emit relationNavigationChanged();
        startRelationQuery(selectedSsa_, RelationQueryKind::DirectChildren);
    }

    void DetailsViewModel::clearRecord() {
        stopRelationQueries();
        latestRelationQueryId_ = 0;
        fields_.clear();
        selectedSsa_.clear();
        relations_.clear();
        graphModel_.buildFromRelations({}, {});
        title_ = "Nenhuma SSA selecionada";
        const bool statusChanged = relationLoading_ || !relationError_.isEmpty();
        relationLoading_ = false;
        relationError_.clear();
        setCurrentRelationIndex(0);
        emit changed();
        if (statusChanged) {
            emit relationStatusChanged();
        }
    }

    DerivadasGraphModel* DetailsViewModel::graphModel() {
        return &graphModel_;
    }

    void DetailsViewModel::rebuildDerivadas(const domain::SsaRecord& record) {
        relations_ = buildLocalRelations(record);
        graphModel_.buildFromRelations(selectedSsa_, relations_);
    }

    void DetailsViewModel::startRelationQuery(const QString& ssaNumber,
                                              const RelationQueryKind kind) {
        stopRelationQueries();
        if (!queryService_ || ssaNumber.isEmpty() || shuttingDown_) {
            const bool statusChanged = relationLoading_ || !relationError_.isEmpty();
            relationLoading_ = false;
            relationError_.clear();
            if (statusChanged) {
                emit relationStatusChanged();
            }
            return;
        }

        auto operation = std::make_unique<RelationQueryOperation>();
        operation->id = ++nextRelationQueryId_;
        operation->ssaNumber = ssaNumber;
        operation->kind = kind;
        operation->state = std::make_shared<RelationQueryState>();
        const auto operationId = operation->id;
        const auto number = operation->ssaNumber.toStdString();
        const auto service = queryService_;
        const auto state = operation->state;
        const auto stopToken = operation->stopSource.get_token();
        connect(&operation->watcher, &QFutureWatcher<void>::finished, this,
                [this, operationId] { finishRelationQuery(operationId); });
        auto* watcher = &operation->watcher;
        latestRelationQueryId_ = operationId;
        relationQueries_.push_back(std::move(operation));
        emit activeOperationsChanged();
        relationLoading_ = true;
        relationError_.clear();
        emit relationStatusChanged();

        watcher->setFuture(QtConcurrent::run([service, number, state, stopToken, kind] {
            try {
                std::scoped_lock lock(state->mutex);
                if (kind == RelationQueryKind::Record) {
                    state->record = service->details(domain::SsaNumber{number}, stopToken);
                } else {
                    state->children =
                        service->derivadasDiretas(domain::SsaNumber{number}, stopToken);
                }
                state->canceled = stopToken.stop_requested();
            } catch (const std::system_error& error) {
                std::scoped_lock lock(state->mutex);
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    state->canceled = true;
                } else {
                    state->error = std::current_exception();
                }
            } catch (...) {
                std::scoped_lock lock(state->mutex);
                state->error = std::current_exception();
            }
        }));
    }

    void DetailsViewModel::finishRelationQuery(const std::uint64_t operationId) {
        const auto found =
            std::ranges::find_if(relationQueries_, [operationId](const auto& operation) {
                return operation->id == operationId;
            });
        if (found == relationQueries_.end()) {
            return;
        }
        auto& operation = **found;
        operation.completed = true;
        emit activeOperationsChanged();
        const bool targetsCurrentRecord =
            operation.kind == RelationQueryKind::Record || operation.ssaNumber == selectedSsa_;
        if (!shuttingDown_ && operation.id == latestRelationQueryId_ && targetsCurrentRecord) {
            std::vector<domain::SsaDerivadaEntry> children;
            std::optional<domain::SsaRecord> record = std::nullopt;
            std::exception_ptr error;
            bool canceled = false;
            {
                std::scoped_lock lock(operation.state->mutex);
                children = std::move(operation.state->children);
                record = std::move(operation.state->record);
                error = operation.state->error;
                canceled = operation.state->canceled || operation.stopSource.stop_requested();
            }
            if (canceled) {
                logAsyncOperationError("Relation query failed after cancellation:", error);
                relationLoading_ = false;
                relationError_.clear();
                emit relationStatusChanged();
            } else if (error) {
                relationLoading_ = false;
                relationError_.clear();
                try {
                    std::rethrow_exception(error);
                } catch (const std::exception& exception) {
                    relationError_ = QString::fromUtf8(exception.what());
                } catch (...) {
                    relationError_ = QStringLiteral("Falha interna ao consultar relacoes");
                }
                emit relationStatusChanged();
            } else if (operation.kind == RelationQueryKind::Record) {
                if (!record) {
                    relationLoading_ = false;
                    relationError_ = QStringLiteral("SSA nao encontrada");
                    emit relationStatusChanged();
                } else {
                    setRecord(*record);
                }
            } else {
                relationLoading_ = false;
                relationError_.clear();
                appendDirectChildren(relations_, children);
                graphModel_.buildFromRelations(selectedSsa_, relations_);
                currentRelationIndex_ = currentRelationIndexFor(relations_, selectedSsa_);
                emit changed();
                emit relationNavigationChanged();
                emit relationStatusChanged();
            }
        }
        QMetaObject::invokeMethod(this, [this] { pruneCompletedQueries(); }, Qt::QueuedConnection);
    }

    void DetailsViewModel::stopRelationQueries() {
        for (const auto& operation : relationQueries_) {
            if (!operation->completed) {
                operation->stopSource.request_stop();
                operation->watcher.cancel();
            }
        }
    }

    void DetailsViewModel::pruneCompletedQueries() {
        std::erase_if(relationQueries_, [](const auto& operation) { return operation->completed; });
    }

    void DetailsViewModel::requestLoadBySsaNumber(const QString& ssaNumber) {
        const auto trimmed = ssaNumber.trimmed();
        if (trimmed.isEmpty()) {
            return;
        }
        startRelationQuery(trimmed, RelationQueryKind::Record);
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

    bool DetailsViewModel::relationLoading() const {
        return relationLoading_;
    }

    QString DetailsViewModel::relationError() const {
        return relationError_;
    }

    void DetailsViewModel::cancel() {
        stopRelationQueries();
    }

    bool DetailsViewModel::hasActiveOperations() const {
        return std::ranges::any_of(relationQueries_,
                                   [](const auto& operation) { return !operation->completed; });
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
            requestLoadBySsaNumber(ssa);
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
