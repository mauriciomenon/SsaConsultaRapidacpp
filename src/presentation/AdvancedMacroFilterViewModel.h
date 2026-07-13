#pragma once

#include "application/SsaExecutadasReportService.h"
#include "domain/SsaTypes.h"
#include "presentation/FilterPanelAdvancedState.h"
#include "presentation/FilterPanelState.h"

#include <QDate>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <vector>

namespace ssa::presentation {

    class AdvancedMacroFilterViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QVariantList options READ options CONSTANT)
        Q_PROPERTY(QString selectedMacro READ selectedMacro WRITE setSelectedMacro NOTIFY changed)
        Q_PROPERTY(QString reportTitle READ reportTitle NOTIFY reportChanged)
        Q_PROPERTY(QString reportText READ reportText NOTIFY reportChanged)
        Q_PROPERTY(QVariantList reportRows READ reportRows NOTIFY reportChanged)
        Q_PROPERTY(bool reportLoading READ reportLoading NOTIFY reportChanged)
        Q_PROPERTY(QString reportError READ reportError NOTIFY reportChanged)

      public:
        using CurrentDate = std::function<QDate()>;

        AdvancedMacroFilterViewModel(
            filterpanel::FilterPanelAdvancedState& advancedState,
            const filterpanel::FilterPanelState& filterState,
            std::shared_ptr<query::SsaQueryService> queryService, QObject* parent = nullptr,
            CurrentDate currentDate = [] { return QDate::currentDate(); });
        ~AdvancedMacroFilterViewModel() override;

        [[nodiscard]] const QVariantList& options() const;
        [[nodiscard]] QString selectedMacro() const;
        void setSelectedMacro(const QString& value);
        [[nodiscard]] QString reportTitle() const;
        [[nodiscard]] QString reportText() const;
        [[nodiscard]] const QVariantList& reportRows() const;
        [[nodiscard]] bool reportLoading() const;
        [[nodiscard]] QString reportError() const;
        [[nodiscard]] bool hasActiveOperations() const;
        void refreshFromState();
        void cancel();

      signals:
        void changed();
        void filterStateChanged();
        void reportChanged();
        void activeOperationsChanged();

      private:
        struct ReportTaskState final {
            std::mutex mutex;
            std::optional<application::ExecutadasReportResult> result = std::nullopt;
            std::exception_ptr error;
            bool canceled{false};
        };

        struct ReportOperation final {
            std::uint64_t id{0};
            bool byDivision{false};
            QFutureWatcher<void> watcher;
            std::shared_ptr<ReportTaskState> state;
            std::stop_source stopSource;
            bool completed{false};
        };

        void applyBaixarPreset();
        void buildExecutadasReport(const QString& value);
        void finishExecutadasReport(std::uint64_t operationId);
        void stopReportOperations();
        void pruneCompletedReports();
        void clearReport();

        filterpanel::FilterPanelAdvancedState& advancedState_;
        const filterpanel::FilterPanelState& filterState_;
        std::shared_ptr<application::SsaExecutadasReportService> reportService_;
        CurrentDate currentDate_;
        QVariantList options_;
        QString selectedMacro_;
        QString reportTitle_;
        QString reportText_;
        QVariantList reportRows_;
        std::vector<std::unique_ptr<ReportOperation>> reportOperations_;
        std::uint64_t latestReportOperationId_{0};
        std::uint64_t nextReportOperationId_{0};
        bool reportLoading_{false};
        bool shuttingDown_{false};
        QString reportError_;
    };

} // namespace ssa::presentation
