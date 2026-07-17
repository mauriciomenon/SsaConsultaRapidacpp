#pragma once

#include "qt/FilesystemPath.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLockFile>

#include <filesystem>
#include <string_view>

namespace ssa::infra::sqlite {

    class SqliteDatabaseWriteLock final {
      public:
        explicit SqliteDatabaseWriteLock(const std::filesystem::path& databasePath)
            : lock_(qt::toQString(pathForDatabase(databasePath))) {
            lock_.setStaleLockTime(0);
            acquired_ = lock_.tryLock(0);
        }

        SqliteDatabaseWriteLock(const SqliteDatabaseWriteLock&) = delete;
        SqliteDatabaseWriteLock& operator=(const SqliteDatabaseWriteLock&) = delete;

        [[nodiscard]] bool acquired() const noexcept {
            return acquired_;
        }

        [[nodiscard]] QLockFile::LockError error() const noexcept {
            return lock_.error();
        }

        [[nodiscard]] std::string_view diagnostic() const noexcept {
            switch (lock_.error()) {
            case QLockFile::NoError:
                return "sqlite database write lock acquisition failed without an error code";
            case QLockFile::LockFailedError:
                return "sqlite database write lock is held by another process";
            case QLockFile::PermissionError:
                return "sqlite database write lock permission denied";
            case QLockFile::UnknownError:
                return "sqlite database write lock reported an unknown filesystem error";
            }
            return "sqlite database write lock reported an invalid error code";
        }

        [[nodiscard]] static std::filesystem::path
        pathForDatabase(const std::filesystem::path& databasePath) {
            std::error_code error;
            auto normalized = std::filesystem::weakly_canonical(databasePath, error);
            if (error) {
                normalized = std::filesystem::absolute(databasePath).lexically_normal();
            }
            const auto digest = QCryptographicHash::hash(qt::toQString(normalized).toUtf8(),
                                                         QCryptographicHash::Sha256)
                                    .toHex()
                                    .toStdString();
            return qt::toFileSystemPath(QDir::tempPath()) / (".ssa_import_db_" + digest + ".lock");
        }

      private:
        QLockFile lock_;
        bool acquired_{false};
    };

} // namespace ssa::infra::sqlite
