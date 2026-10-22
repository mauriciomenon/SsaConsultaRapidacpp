#include "DesktopLogSink.h"

#include "platform/RotatingLogWriter.h"
#include "presentation/RecentLogModel.h"

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>

#include <condition_variable>
#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

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

        explicit HandlerState(const std::filesystem::path& configDirectory)
            : writer_(configDirectory / "logs" / "ssa.log", 1024ULL * 1024ULL, 3) {}

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
                publish({QStringLiteral("Error"), QStringLiteral("Log"),
                         QStringLiteral("Failed to write rotating log"),
                         QString::fromUtf8(exception.what())});
            }
            publish({severity, QStringLiteral("Qt"), message, detail});
        }

        // Startup logs before the view model exists; they reach its log list once attached.
        void attachModel(ssa::presentation::RecentLogModel& model) {
            std::vector<Entry> pending;
            {
                const std::scoped_lock lock(modelMutex_);
                model_ = &model;
                attached_ = true;
                pending.swap(pending_);
            }
            for (auto& entry : pending) {
                publish(std::move(entry));
            }
        }

        QtMessageHandler previousHandler{nullptr};

      private:
        struct Entry final {
            QString severity;
            QString source;
            QString message;
            QString detail;
        };

        void releaseHandler() {
            {
                const std::scoped_lock lock(handlerMutex_);
                --handlersInFlight_;
            }
            handlerCondition_.notify_all();
        }

        void publish(Entry entry) {
            QPointer<ssa::presentation::RecentLogModel> model;
            {
                const std::scoped_lock lock(modelMutex_);
                if (!attached_) {
                    pending_.push_back(std::move(entry));
                    return;
                }
                model = model_;
            }
            if (model == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                model.data(),
                [model, entry = std::move(entry)] {
                    if (model != nullptr) {
                        model->append(entry.severity, entry.source, entry.message, entry.detail);
                    }
                },
                Qt::QueuedConnection);
        }

        ssa::platform::RotatingLogWriter writer_;
        std::mutex modelMutex_;
        QPointer<ssa::presentation::RecentLogModel> model_;
        bool attached_{false};
        std::vector<Entry> pending_;
        std::mutex handlerMutex_;
        std::condition_variable handlerCondition_;
        std::size_t handlersInFlight_{0};
    };

    std::mutex DesktopLogSink::handlerMutex_;
    std::shared_ptr<DesktopLogSink::HandlerState> DesktopLogSink::active_;

    DesktopLogSink::DesktopLogSink(const std::filesystem::path& configDirectory)
        : state_(std::make_shared<HandlerState>(configDirectory)) {
        const std::scoped_lock lock(handlerMutex_);
        if (active_) {
            throw std::logic_error("desktop log sink is already installed");
        }
        state_->previousHandler = qInstallMessageHandler(&DesktopLogSink::messageHandler);
        active_ = state_;
    }

    void DesktopLogSink::attachModel(ssa::presentation::RecentLogModel& model) {
        state_->attachModel(model);
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
