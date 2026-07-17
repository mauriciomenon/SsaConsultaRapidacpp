#include "DesktopLogSink.h"

#include "platform/RotatingLogWriter.h"
#include "presentation/RecentLogModel.h"

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>

#include <condition_variable>
#include <exception>
#include <stdexcept>

namespace ssa::app::desktop {

    class DesktopLogSink::HandlerState final {
      public:
        class Reservation final {
          public:
            explicit Reservation(std::shared_ptr<HandlerState> state) noexcept
                : state_(std::move(state)) {}

            ~Reservation() {
                state_->releaseHandler();
            }

            Reservation(const Reservation&) = delete;
            Reservation& operator=(const Reservation&) = delete;

          private:
            std::shared_ptr<HandlerState> state_;
        };

        HandlerState(const std::filesystem::path& configDirectory,
                     ssa::presentation::RecentLogModel& model)
            : writer_(configDirectory / "logs" / "ssa.log", 1024ULL * 1024ULL, 3), model_(&model) {}

        void reserveHandler() {
            const std::scoped_lock lock(handlerMutex_);
            ++handlersInFlight_;
        }

        void waitForHandlers() {
            std::unique_lock lock(handlerMutex_);
            handlerCondition_.wait(lock, [this] { return handlersInFlight_ == 0; });
        }

        void record(const QtMsgType type, const QMessageLogContext& context,
                    const QString& message) {
            if (type == QtDebugMsg) {
                return;
            }
            const QString severity = type == QtInfoMsg      ? QStringLiteral("Info")
                                     : type == QtWarningMsg ? QStringLiteral("Warning")
                                                            : QStringLiteral("Error");
            QString detail =
                QString::fromUtf8(context.category == nullptr ? "default" : context.category);
            if (context.file != nullptr) {
                detail += QStringLiteral(" ") + QString::fromUtf8(context.file) +
                          QStringLiteral(":") + QString::number(context.line);
            }
            const QString line = QStringLiteral("%1 [%2] %3 | %4")
                                     .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                          severity, message, detail);
            try {
                writer_.append(line.toUtf8().toStdString());
            } catch (const std::exception& exception) {
                appendFailure(QString::fromUtf8(exception.what()));
            }
            append(severity, message, detail);
        }

        QtMessageHandler previousHandler{nullptr};

      private:
        void releaseHandler() {
            {
                const std::scoped_lock lock(handlerMutex_);
                --handlersInFlight_;
            }
            handlerCondition_.notify_all();
        }

        void appendFailure(const QString& diagnostic) const {
            const QPointer model{model_};
            if (model == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                model.data(),
                [model, diagnostic] {
                    if (model != nullptr) {
                        model->append(QStringLiteral("Error"), QStringLiteral("Log"),
                                      QStringLiteral("Failed to write rotating log"), diagnostic);
                    }
                },
                Qt::QueuedConnection);
        }

        void append(const QString& severity, const QString& message, const QString& detail) const {
            const QPointer model{model_};
            if (model == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                model.data(),
                [model, severity, message, detail] {
                    if (model != nullptr) {
                        model->append(severity, QStringLiteral("Qt"), message, detail);
                    }
                },
                Qt::QueuedConnection);
        }

        ssa::platform::RotatingLogWriter writer_;
        QPointer<ssa::presentation::RecentLogModel> model_;
        std::mutex handlerMutex_;
        std::condition_variable handlerCondition_;
        std::size_t handlersInFlight_{0};
    };

    std::mutex DesktopLogSink::handlerMutex_;
    std::shared_ptr<DesktopLogSink::HandlerState> DesktopLogSink::active_;

    DesktopLogSink::DesktopLogSink(const std::filesystem::path& configDirectory,
                                   ssa::presentation::RecentLogModel& model)
        : state_(std::make_shared<HandlerState>(configDirectory, model)) {
        const std::scoped_lock lock(handlerMutex_);
        if (active_) {
            throw std::logic_error("desktop log sink is already installed");
        }
        state_->previousHandler = qInstallMessageHandler(&DesktopLogSink::messageHandler);
        active_ = state_;
    }

    DesktopLogSink::~DesktopLogSink() {
        std::shared_ptr<HandlerState> state;
        {
            const std::scoped_lock lock(handlerMutex_);
            if (active_ == state_) {
                qInstallMessageHandler(state_->previousHandler);
                state = std::move(active_);
            }
        }
        if (state) {
            state->waitForHandlers();
        }
    }

    void DesktopLogSink::messageHandler(const QtMsgType type, const QMessageLogContext& context,
                                        const QString& message) {
        std::shared_ptr<HandlerState> state;
        {
            const std::scoped_lock lock(handlerMutex_);
            state = active_;
            if (state) {
                state->reserveHandler();
            }
        }
        if (!state) {
            return;
        }
        const HandlerState::Reservation reservation{state};
        state->record(type, context, message);
        if (state->previousHandler != nullptr) {
            state->previousHandler(type, context, message);
        }
    }

} // namespace ssa::app::desktop
