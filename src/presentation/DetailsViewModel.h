#pragma once

#include "domain/SsaTypes.h"
#include "ports/ISsaBrowsePort.h"
#include "presentation/DerivadasGraphModel.h"
#include "presentation/DetailsFieldsModel.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <vector>

namespace ssa::presentation {

    class DetailsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString title READ title NOTIFY changed)
        Q_PROPERTY(DetailsFieldsModel* fields READ fields NOTIFY changed)
        Q_PROPERTY(QString selectedSsaNumber READ selectedSsaNumber NOTIFY changed)
        Q_PROPERTY(int fieldCount READ fieldCount NOTIFY changed)
        Q_PROPERTY(QVariantList relations READ relations NOTIFY changed)
        Q_PROPERTY(int relationCount READ relationCount NOTIFY changed)
        Q_PROPERTY(bool relationLoading READ relationLoading NOTIFY relationStatusChanged)
        Q_PROPERTY(QString relationError READ relationError NOTIFY relationStatusChanged)
        Q_PROPERTY(DerivadasGraphModel* graphModel READ graphModel CONSTANT)
        Q_PROPERTY(
            int currentRelationIndex READ currentRelationIndex NOTIFY relationNavigationChanged)
        Q_PROPERTY(
            bool canSelectNextRelation READ canSelectNextRelation NOTIFY relationNavigationChanged)
        Q_PROPERTY(bool canSelectPreviousRelation READ canSelectPreviousRelation NOTIFY
                       relationNavigationChanged)

      public:
        explicit DetailsViewModel(QObject* parent = nullptr);
        explicit DetailsViewModel(std::shared_ptr<ports::ISsaBrowsePort> browsePort,
                                  QObject* parent = nullptr);
        ~DetailsViewModel() override;

        [[nodiscard]] QString title() const;
        [[nodiscard]] DetailsFieldsModel* fields();
        [[nodiscard]] int fieldCount() const;
        void setRecord(const domain::SsaRecord& record);
        void clearRecord();
        Q_INVOKABLE void requestLoadBySsaNumber(const QString& ssaNumber);
        [[nodiscard]] int currentRelationIndex() const;
        [[nodiscard]] bool canSelectNextRelation() const;
        [[nodiscard]] bool canSelectPreviousRelation() const;
        [[nodiscard]] bool relationLoading() const;
        [[nodiscard]] QString relationError() const;
        [[nodiscard]] QString selectedSsa() const;
        [[nodiscard]] QString selectedSsaNumber() const;
        [[nodiscard]] QVariantList relations() const;
        [[nodiscard]] int relationCount() const;
        [[nodiscard]] DerivadasGraphModel* graphModel();
        [[nodiscard]] bool hasActiveOperations() const;
        void cancel();

      public slots:
        void selectNextRelation();
        void selectPreviousRelation();

      signals:
        void changed();
        void relationNavigationChanged();
        void relationStatusChanged();
        void activeOperationsChanged();

      private:
        enum class RelationQueryKind { DirectChildren, Record };

        struct RelationQueryState final {
            std::mutex mutex;
            std::vector<domain::SsaDerivadaEntry> children;
            std::optional<domain::SsaRecord> record = std::nullopt;
            std::exception_ptr error;
            bool canceled{false};
        };

        struct RelationQueryOperation final {
            std::uint64_t id{0};
            QString ssaNumber;
            RelationQueryKind kind{RelationQueryKind::DirectChildren};
            QFutureWatcher<void> watcher;
            std::shared_ptr<RelationQueryState> state;
            std::stop_source stopSource;
            bool completed{false};
        };

        void rebuildDerivadas(const domain::SsaRecord& record);
        void startRelationQuery(const QString& ssaNumber, RelationQueryKind kind);
        void finishRelationQuery(std::uint64_t operationId);
        void stopRelationQueries();
        void pruneCompletedQueries();
        void setCurrentRelationIndex(int index);
        void loadRelation(int index);

        QString title_;
        DetailsFieldsModel fields_;
        QString selectedSsa_;
        QVariantList relations_;
        DerivadasGraphModel graphModel_;
        std::shared_ptr<ports::ISsaBrowsePort> browsePort_;
        int currentRelationIndex_{0};
        std::vector<std::unique_ptr<RelationQueryOperation>> relationQueries_;
        std::uint64_t latestRelationQueryId_{0};
        std::uint64_t nextRelationQueryId_{0};
        bool relationLoading_{false};
        bool shuttingDown_{false};
        QString relationError_;
    };

} // namespace ssa::presentation
