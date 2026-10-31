#include "infra/import/CancelableFileCopy.h"

#include "infra/import/SourceFileCapture.h"
#include "qt/FilesystemPath.h"

#include <QTemporaryFile>

#include <exception>
#include <stdexcept>
#include <system_error>

namespace ssa::infra::importing {

    std::optional<FileIdentitySnapshot> inspectFileIdentity(const std::filesystem::path& path,
                                                            std::error_code& error) {
        const auto snapshot = inspectSourceFileIdentity(path, error);
        if (!snapshot) {
            return std::nullopt;
        }
        return FileIdentitySnapshot{snapshot->identity, snapshot->size};
    }

    FileCopyResult copyFileAtomically(const FileCopyRequest& request,
                                      const std::stop_token stopToken) {
        if (stopToken.stop_requested()) {
            return {FileCopyStatus::Canceled, {}};
        }
        std::error_code error;
        std::filesystem::create_directories(request.destination.parent_path(), error);
        if (error) {
            return {FileCopyStatus::Failed, "cannot create copy destination: " + error.message()};
        }

        auto temporaryTemplate = request.destination;
        temporaryTemplate += ".XXXXXX.part";
        QTemporaryFile output(qt::toQString(temporaryTemplate));
        const auto failCopy = [&output](FileCopyResult result) {
            output.close();
            if (!output.fileName().isEmpty() && output.exists() && !output.remove()) {
                if (!result.diagnostic.empty()) {
                    result.diagnostic += "; ";
                }
                result.diagnostic +=
                    "staging cleanup failed operation=remove_copy_temporary path=" +
                    qt::toUtf8(qt::toFileSystemPath(output.fileName())) +
                    " error=" + output.errorString().toStdString() + " pending=true";
                result.status = FileCopyStatus::CleanupFailed;
            }
            return result;
        };
        if (!output.open()) {
            return failCopy({FileCopyStatus::Failed, "cannot open staged file copy"});
        }

        std::filesystem::file_time_type sourceModified;
        SourceFileSnapshot sourceSnapshot;
        try {
            sourceSnapshot = captureSourceFile(
                request.source,
                [&output](const std::span<const char> bytes) {
                    const auto size = static_cast<qint64>(bytes.size());
                    if (output.write(bytes.data(), size) != size) {
                        throw std::runtime_error("cannot write staged file copy");
                    }
                },
                stopToken, 128ULL * 1024ULL * 1024ULL,
                [&request] {
                    if (!request.afterFirstChunkWritten) {
                        return;
                    }
                    try {
                        request.afterFirstChunkWritten();
                    } catch (const std::exception& exception) {
                        throw std::runtime_error("staged file copy callback failed: " +
                                                 std::string{exception.what()});
                    } catch (...) {
                        throw std::runtime_error(
                            "staged file copy callback failed with non-standard exception");
                    }
                });
            sourceModified = sourceSnapshot.fileModificationTime();
        } catch (const SourceReadDeferred& deferred) {
            return failCopy({FileCopyStatus::Deferred, deferred.what(), deferred.reason()});
        } catch (const std::system_error& exception) {
            if (exception.code() == std::errc::operation_canceled) {
                return failCopy({FileCopyStatus::Canceled, {}});
            }
            return failCopy({FileCopyStatus::Failed, exception.what()});
        } catch (const std::exception& exception) {
            return failCopy({FileCopyStatus::Failed, exception.what()});
        }
        if (!output.flush()) {
            return failCopy({FileCopyStatus::Failed, "cannot flush staged file copy"});
        }
        output.close();
        if (stopToken.stop_requested()) {
            return failCopy({FileCopyStatus::Canceled, {}});
        }
        // Preserve the source handle's mtime, not the temporary copy's creation time.
        std::filesystem::last_write_time(qt::toFileSystemPath(output.fileName()), sourceModified,
                                         error);
        if (error) {
            return failCopy(
                {FileCopyStatus::Failed, "cannot preserve staged file mtime: " + error.message()});
        }
        if (!output.rename(request.destination)) {
            return failCopy({FileCopyStatus::Failed, "cannot publish staged file copy: " +
                                                         output.errorString().toStdString()});
        }
        output.setAutoRemove(false);
        return {FileCopyStatus::Succeeded, {}, {}, std::move(sourceSnapshot)};
    }

} // namespace ssa::infra::importing
