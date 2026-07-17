#include "DesktopLogSink.h"

#include "presentation/RecentLogModel.h"

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>

#include <exception>
#include <stdexcept>

namespace ssa::app::desktop {

    std::mutex DesktopLogSink::handlerMutex_;
    DesktopLogSink* DesktopLogSink::active_ = nullptr;

    DesktopLogSink::DesktopLogSink(const std::filesystem::path& configDirectory,
                                   ssa::presentation::RecentLogModel& model)
        : writer_(configDirectory / "logs" / "ssa.log", 1024 * 1024, 3), model_(model) {
        const std::scoped_lock lock(handlerMutex_);
        if (active_ != nullptr) {
            throw std::logic_error("desktop log sink is already installed");
        }
        active_ = this;
        previousHandler_ = qInstallMessageHandler(&DesktopLogSink::messageHandler);
    }

    DesktopLogSink::~DesktopLogSink() {
        const std::scoped_lock lock(handlerMutex_);
        if (active_ == this) {
            qInstallMessageHandler(previousHandler_);
            active_ = nullptr;
        }
    }

    void DesktopLogSink::messageHandler(const QtMsgType type, const QMessageLogContext& context,
                                        const QString& message) {
        QtMessageHandler previous = nullptr;
        {
            const std::scoped_lock lock(handlerMutex_);
            if (active_ != nullptr) {
                active_->record(type, context, message);
                previous = active_->previousHandler_;
            }
        }
        if (previous != nullptr) {
            previous(type, context, message);
        }
    }

    void DesktopLogSink::record(const QtMsgType type, const QMessageLogContext& context,
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
            detail += QStringLiteral(" ") + QString::fromUtf8(context.file) + QStringLiteral(":") +
                      QString::number(context.line);
        }
        const QString line = QStringLiteral("%1 [%2] %3 | %4")
                                 .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                      severity, message, detail);
        try {
            writer_.append(line.toUtf8().toStdString());
        } catch (const std::exception& exception) {
            const QPointer model{&model_};
            QMetaObject::invokeMethod(
                &model_,
                [model, diagnostic = QString::fromUtf8(exception.what())] {
                    if (model != nullptr) {
                        model->append(QStringLiteral("Error"), QStringLiteral("Log"),
                                      QStringLiteral("Failed to write rotating log"), diagnostic);
                    }
                },
                Qt::QueuedConnection);
        }

        const QPointer model{&model_};
        QMetaObject::invokeMethod(
            &model_,
            [model, severity, message, detail] {
                if (model != nullptr) {
                    model->append(severity, QStringLiteral("Qt"), message, detail);
                }
            },
            Qt::QueuedConnection);
    }

} // namespace ssa::app::desktop
