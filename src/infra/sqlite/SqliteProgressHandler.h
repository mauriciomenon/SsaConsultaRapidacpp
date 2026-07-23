#pragma once

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <climits>
#include <semaphore>
#include <stop_token>
#include <system_error>

namespace ssa::infra::sqlite {

    using SqliteSynchronizationSemaphore = std::counting_semaphore<INT_MAX>;

    inline void throwIfCanceled(const std::stop_token& stopToken) {
        if (stopToken.stop_requested()) {
            throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                    "sqlite query canceled");
        }
    }

    class SqliteBusyHandler final {
      public:
        explicit SqliteBusyHandler(
            sqlite3* db, std::stop_token stopToken,
            std::chrono::milliseconds maxWait = std::chrono::milliseconds{3000},
            SqliteSynchronizationSemaphore* busyEntered = nullptr,
            std::atomic_flag* externalBusyReported = nullptr)
            : db_(db), stopToken_(std::move(stopToken)),
              deadline_(std::chrono::steady_clock::now() + maxWait), busyEntered_(busyEntered),
              externalBusyReported_(externalBusyReported) {
            sqlite3_busy_handler(db_, &SqliteBusyHandler::shouldRetry, this);
        }

        ~SqliteBusyHandler() {
            disable();
        }

        SqliteBusyHandler(const SqliteBusyHandler&) = delete;
        SqliteBusyHandler& operator=(const SqliteBusyHandler&) = delete;

        [[nodiscard]] const std::atomic_bool* cancellationObserved() const noexcept {
            return &cancellationObserved_;
        }

        void disable() noexcept {
            if (db_ == nullptr) {
                return;
            }
            sqlite3_busy_handler(db_, nullptr, nullptr);
            db_ = nullptr;
        }

      private:
        static constexpr int kRetryDelayMs = 5;

        static int shouldRetry(void* context, const int) noexcept {
            auto* handler = static_cast<SqliteBusyHandler*>(context);
            auto& busyReported = handler->externalBusyReported_ != nullptr
                                     ? *handler->externalBusyReported_
                                     : handler->busyReported_;
            if (handler->busyEntered_ != nullptr &&
                !busyReported.test_and_set(std::memory_order_relaxed)) {
                handler->busyEntered_->release();
            }
            const auto now = std::chrono::steady_clock::now();
            if (handler->stopToken_.stop_requested() || now >= handler->deadline_) {
                if (handler->stopToken_.stop_requested()) {
                    handler->cancellationObserved_.store(true, std::memory_order_relaxed);
                }
                return 0;
            }
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(handler->deadline_ - now);
            if (remaining < std::chrono::milliseconds{1}) {
                return 0;
            }
            sqlite3_sleep(remaining < std::chrono::milliseconds{kRetryDelayMs}
                              ? static_cast<int>(remaining.count())
                              : kRetryDelayMs);
            if (handler->stopToken_.stop_requested()) {
                handler->cancellationObserved_.store(true, std::memory_order_relaxed);
                return 0;
            }
            return std::chrono::steady_clock::now() < handler->deadline_ ? 1 : 0;
        }

        sqlite3* db_ = nullptr;
        std::stop_token stopToken_;
        std::chrono::steady_clock::time_point deadline_;
        SqliteSynchronizationSemaphore* busyEntered_ = nullptr;
        std::atomic_flag* externalBusyReported_ = nullptr;
        std::atomic_bool cancellationObserved_{false};
        std::atomic_flag busyReported_ = ATOMIC_FLAG_INIT;
    };

    class SqliteProgressHandler final {
      public:
        explicit SqliteProgressHandler(sqlite3* db, std::stop_token stopToken,
                                       SqliteSynchronizationSemaphore* progressEntered = nullptr)
            : db_(db), stopToken_(std::move(stopToken)), progressEntered_(progressEntered) {
            throwIfCanceled(stopToken_);
            if (stopToken_.stop_possible()) {
                sqlite3_progress_handler(db_, 1000, &SqliteProgressHandler::shouldInterrupt, this);
                installed_ = true;
            }
        }

        ~SqliteProgressHandler() {
            disable();
        }

        SqliteProgressHandler(const SqliteProgressHandler&) = delete;
        SqliteProgressHandler& operator=(const SqliteProgressHandler&) = delete;

        void disable() noexcept {
            if (!installed_) {
                return;
            }
            sqlite3_progress_handler(db_, 0, nullptr, nullptr);
            installed_ = false;
        }

      private:
        static int shouldInterrupt(void* context) noexcept {
            auto* handler = static_cast<SqliteProgressHandler*>(context);
            if (handler->progressEntered_ != nullptr &&
                !handler->progressReported_.test_and_set(std::memory_order_relaxed)) {
                handler->progressEntered_->release();
            }
            return handler->stopToken_.stop_requested() ? 1 : 0;
        }

        sqlite3* db_ = nullptr;
        std::stop_token stopToken_;
        SqliteSynchronizationSemaphore* progressEntered_ = nullptr;
        std::atomic_flag progressReported_ = ATOMIC_FLAG_INIT;
        bool installed_ = false;
    };

} // namespace ssa::infra::sqlite
