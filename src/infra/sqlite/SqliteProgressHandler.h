#pragma once

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <semaphore>
#include <stop_token>
#include <system_error>

namespace ssa::infra::sqlite {

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
            std::binary_semaphore* busyEntered = nullptr)
            : db_(db), stopToken_(std::move(stopToken)),
              maxRetries_(static_cast<int>(maxWait.count() / kRetryDelayMs)),
              busyEntered_(busyEntered) {
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

        static int shouldRetry(void* context, const int retryCount) noexcept {
            auto* handler = static_cast<SqliteBusyHandler*>(context);
            if (handler->busyEntered_ != nullptr &&
                !handler->busyReported_.test_and_set(std::memory_order_relaxed)) {
                handler->busyEntered_->release();
            }
            if (handler->stopToken_.stop_requested() || retryCount >= handler->maxRetries_) {
                if (handler->stopToken_.stop_requested()) {
                    handler->cancellationObserved_.store(true, std::memory_order_relaxed);
                }
                return 0;
            }
            sqlite3_sleep(kRetryDelayMs);
            if (handler->stopToken_.stop_requested()) {
                handler->cancellationObserved_.store(true, std::memory_order_relaxed);
                return 0;
            }
            return 1;
        }

        sqlite3* db_ = nullptr;
        std::stop_token stopToken_;
        int maxRetries_ = 0;
        std::binary_semaphore* busyEntered_ = nullptr;
        std::atomic_bool cancellationObserved_{false};
        std::atomic_flag busyReported_ = ATOMIC_FLAG_INIT;
    };

    class SqliteProgressHandler final {
      public:
        explicit SqliteProgressHandler(sqlite3* db, std::stop_token stopToken,
                                       std::binary_semaphore* progressEntered = nullptr)
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
        std::binary_semaphore* progressEntered_ = nullptr;
        std::atomic_flag progressReported_ = ATOMIC_FLAG_INIT;
        bool installed_ = false;
    };

} // namespace ssa::infra::sqlite
