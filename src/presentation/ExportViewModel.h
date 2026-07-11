#pragma once

#include "application/SsaWorkflowService.h"
#include "domain/SsaTypes.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QUrl>

#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>

namespace ssa::presentation {

    class ExportViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastSucceeded READ lastSucceeded NOTIFY lastResultChanged)
        Q_PROPERTY(QString lastStatus READ lastStatus NOTIFY lastResultChanged)
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)

      public:
        using RequestFactory = std::function<domain::SsaPageRequest()>;

        ExportViewModel(std::shared_ptr<application::SsaWorkflowService> workflows,
                        RequestFactory requestFactory, QThreadPool* exportThreadPool = nullptr,
                        QObject* parent = nullptr);
        ~ExportViewModel() override;

        [[nodiscard]] QString lastMessage() const;
        [[nodiscard]] bool lastSucceeded() const;
        [[nodiscard]] QString lastStatus() const;
        [[nodiscard]] bool running() const;

      signals:
        void lastResultChanged();
        void runningChanged();

      public slots:
        void exportFilteredList(const QUrl& outputUrl);

      private:
        struct ResultState final {
            std::mutex mutex;
            std::optional<ports::WorkflowResult> result;
            std::exception_ptr error;
        };

        void finishExport();
        void applyResult(const ports::WorkflowResult& result);
        void setExportState(QString status, QString message, bool succeeded);

        std::shared_ptr<application::SsaWorkflowService> workflows_;
        RequestFactory requestFactory_;
        std::unique_ptr<QThreadPool> ownedExportThreadPool_;
        QThreadPool* exportThreadPool_{nullptr};
        QFutureWatcher<void> exportWatcher_;
        std::shared_ptr<ResultState> resultState_;
        std::stop_source exportStopSource_;
        QString lastMessage_;
        QString lastStatus_{"idle"};
        bool lastSucceeded_{false};
        bool running_{false};
    };

} // namespace ssa::presentation
