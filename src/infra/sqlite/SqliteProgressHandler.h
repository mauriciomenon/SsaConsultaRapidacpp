#pragma once

#include <sqlite3.h>

#include <atomic>
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
            if (installed_) {
                sqlite3_progress_handler(db_, 0, nullptr, nullptr);
            }
        }

        SqliteProgressHandler(const SqliteProgressHandler&) = delete;
        SqliteProgressHandler& operator=(const SqliteProgressHandler&) = delete;

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
