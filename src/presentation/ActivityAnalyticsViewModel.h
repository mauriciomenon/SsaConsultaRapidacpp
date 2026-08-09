#pragma once

#include "application/ActivityAnalyticsService.h"
#include "domain/ActivityAnalyticsTypes.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <variant>
#include <vector>

namespace ssa::presentation {

    class ActivityAnalyticsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
        Q_PROPERTY(bool canceling READ canceling NOTIFY stateChanged)
        Q_PROPERTY(bool canCancel READ canCancel NOTIFY stateChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(QVariantMap dashboard READ dashboard NOTIFY dashboardChanged)
        Q_PROPERTY(QVariantMap customSeries READ customSeries NOTIFY customSeriesChanged)
        Q_PROPERTY(QVariantMap dimensionValues READ dimensionValues NOTIFY dimensionValuesChanged)
        Q_PROPERTY(QVariantList availability READ availability NOTIFY availabilityChanged)
        Q_PROPERTY(
            QVariant warningWindowDays READ warningWindowDays NOTIFY warningWindowDaysChanged)

      public:
        enum class RequestKind : std::uint8_t {
            Dashboard,
            CustomSeries,
            DimensionValues,
            Availability,
            WarningWindowLoad,
            WarningWindowSave,
        };
        Q_ENUM(RequestKind)

        explicit ActivityAnalyticsViewModel(
            std::shared_ptr<const application::ActivityAnalyticsService> service,
            QObject* parent = nullptr);
        ~ActivityAnalyticsViewModel() override;

        [[nodiscard]] bool loading() const noexcept;
        [[nodiscard]] bool canceling() const noexcept;
        [[nodiscard]] bool canCancel() const noexcept;
        [[nodiscard]] QString errorMessage() const;
        [[nodiscard]] const QVariantMap& dashboard() const noexcept;
        [[nodiscard]] const QVariantMap& customSeries() const noexcept;
        [[nodiscard]] const QVariantMap& dimensionValues() const noexcept;
        [[nodiscard]] const QVariantList& availability() const noexcept;
        [[nodiscard]] const QVariant& warningWindowDays() const noexcept;
        [[nodiscard]] bool hasActiveOperations() const noexcept;

        void loadDashboard(const domain::AnalyticsPeriod& reportPeriod,
                           const domain::AnalyticsPeriod& historyPeriod,
                           std::optional<int> warningWindowDays);
        void loadCustomSeries(domain::AnalyticsRequest request, int categorySort = 0);
        void loadDimensionValues(domain::AnalyticsRequest request);
        Q_INVOKABLE [[nodiscard]] QVariantMap currentMonthSelection() const;
        Q_INVOKABLE [[nodiscard]] QVariantMap calendarMonthSelection(int year, int month) const;
        Q_INVOKABLE [[nodiscard]] QVariantMap isoMonthSelection(int year, int month) const;
        Q_INVOKABLE [[nodiscard]] QVariantMap yearToDateSelection() const;
        Q_INVOKABLE [[nodiscard]] QVariantMap currentIsoWeekSelection() const;
        Q_INVOKABLE [[nodiscard]] QVariantMap currentIsoMonthSelection() const;
        Q_INVOKABLE void clearCustomSeries();
        Q_INVOKABLE [[nodiscard]] QString customChartTitle(const QVariantMap& selection) const;
        Q_INVOKABLE bool writeExportFile(const QString& path, const QString& content) const;
        Q_INVOKABLE bool requestDashboard(const QVariantMap& selection);
        Q_INVOKABLE bool requestCustomSeries(const QVariantMap& selection);
        Q_INVOKABLE bool requestDimensionValues(const QVariantMap& selection);
        Q_INVOKABLE void loadAvailability();
        Q_INVOKABLE void loadWarningWindowDays();
        Q_INVOKABLE bool saveWarningWindowDays(int days);
        Q_INVOKABLE void cancel();
        void replaceService(std::shared_ptr<const application::ActivityAnalyticsService> service);
        void invalidateAfterImport();

      signals:
        void stateChanged();
        void errorMessageChanged();
        void dashboardChanged();
        void customSeriesChanged();
        void dimensionValuesChanged();
        void availabilityChanged();
        void warningWindowDaysChanged();
        void warningWindowLoadFinished(bool successful);
        void succeeded(ssa::presentation::ActivityAnalyticsViewModel::RequestKind kind);
        void canceled();
        void replaced();
        void failed(QString message);
        void metricUnavailable(int metric, QString reason);
        void activeOperationsChanged();
        void invalidated();

      private:
        struct UnavailableMetric final {
            domain::AnalyticsMetric metric{domain::AnalyticsMetric::Registered};
            QString reason;
        };

        struct DashboardPayload final {
            QVariantMap model;
            std::vector<UnavailableMetric> unavailableMetrics;
        };

        struct CustomSeriesPayload final {
            QVariantMap model;
            domain::AnalyticsMetric metric{domain::AnalyticsMetric::Registered};
            bool unavailable{false};
            QString unavailableReason;
        };

        struct WarningWindowPayload final {
            std::optional<int> days;
        };

        using TaskResult =
            std::variant<DashboardPayload, CustomSeriesPayload, domain::AnalyticsDimensionValues,
                         std::vector<domain::AnalyticsMetricAvailability>, WarningWindowPayload>;

        struct TaskState final {
            std::mutex mutex;
            std::optional<TaskResult> result;
            std::exception_ptr error;
            bool canceled{false};
        };

        struct Operation final {
            std::uint64_t id{0};
            std::uint64_t generation{0};
            RequestKind kind{RequestKind::CustomSeries};
            QFutureWatcher<void> watcher;
            std::shared_ptr<TaskState> state;
            std::stop_source stopSource;
            bool explicitlyCanceled{false};
            bool completed{false};
        };

        using Work = std::function<TaskResult(const std::stop_token&)>;

        void start(RequestKind kind, Work work);
        void finish(std::uint64_t operationId);
        void stop(Operation& operation);
        void stopAll();
        void pruneCompleted();
        void publish(const Operation& operation, TaskResult result);
        void clearModels(bool clearSettings);
        void invalidate(bool clearSettings);
        void rejectSelection(const std::exception& error);
        void setState(bool loading, bool canceling);
        void setErrorMessage(QString message);

        std::shared_ptr<const application::ActivityAnalyticsService> service_;
        std::vector<std::unique_ptr<Operation>> operations_;
        std::uint64_t latestOperationId_{0};
        std::uint64_t nextOperationId_{0};
        std::uint64_t generation_{0};
        QVariantMap dashboard_;
        QVariantMap customSeries_;
        QVariantMap dimensionValues_;
        QVariantList availability_;
        QVariant warningWindowDays_;
        QString errorMessage_;
        bool loading_{false};
        bool canceling_{false};
        bool shuttingDown_{false};
    };

} // namespace ssa::presentation
