#include "infra/import/SourceFileCapture.h"

#include <QDateTime>

#include <algorithm>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ssa::infra::importing {
    namespace {

        class SourceHandle final {
          public:
            explicit SourceHandle(const std::filesystem::path& path, const bool inspectOnly)
#ifdef _WIN32
                : handle_(CreateFileW(
                      path.c_str(), inspectOnly ? FILE_READ_ATTRIBUTES : GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_DELETE | (inspectOnly ? FILE_SHARE_WRITE : 0),
                      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr)) {
                if (handle_ == INVALID_HANDLE_VALUE && !inspectOnly &&
                    GetLastError() == ERROR_SHARING_VIOLATION) {
                    // A writer can retain stale timestamps until close. Verify the bytes twice.
                    sharedWrite_ = true;
                    handle_ =
                        CreateFileW(path.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
                }
                if (handle_ == INVALID_HANDLE_VALUE) {
                    const auto error = GetLastError();
                    if (sharedWrite_ && error == ERROR_SHARING_VIOLATION) {
                        // An exclusive writer denies even shared reads: retry on the next run.
                        throw SourceReadDeferred{"source_in_use",
                                                 "win32_error=" + std::to_string(error)};
                    }
                    throw std::system_error(static_cast<int>(error), std::system_category(),
                                            "cannot open source file");
                }
#else
                : handle_(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK)) {
                (void)inspectOnly;
                if (handle_ < 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "cannot open source file");
                }
#endif
            }

            ~SourceHandle() {
#ifdef _WIN32
                if (handle_ != INVALID_HANDLE_VALUE) {
                    CloseHandle(handle_);
                }
#else
                if (handle_ >= 0) {
                    ::close(handle_);
                }
#endif
            }

            SourceHandle(const SourceHandle&) = delete;
            SourceHandle& operator=(const SourceHandle&) = delete;

            [[nodiscard]] SourceFileSnapshot snapshot() const {
#ifdef _WIN32
                BY_HANDLE_FILE_INFORMATION information{};
                if (GetFileInformationByHandle(handle_, &information) == 0) {
                    throw std::system_error(static_cast<int>(GetLastError()),
                                            std::system_category(), "cannot inspect source handle");
                }
                if ((information.dwFileAttributes &
                     (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
                    throw std::system_error(
                        std::make_error_code(std::errc::operation_not_permitted),
                        "source is not a regular file");
                }
                const auto index = (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
                                   information.nFileIndexLow;
                const auto size = (static_cast<std::uint64_t>(information.nFileSizeHigh) << 32) |
                                  information.nFileSizeLow;
                const auto ticks =
                    (static_cast<std::uint64_t>(information.ftLastWriteTime.dwHighDateTime) << 32) |
                    information.ftLastWriteTime.dwLowDateTime;
                // FILETIME counts 100 ns intervals since 1601; sys_time starts in 1970.
                const auto modified =
                    std::chrono::sys_time<std::chrono::seconds>{std::chrono::seconds{
                        static_cast<std::int64_t>(ticks / 10'000'000) - 11'644'473'600LL}};
                const auto subseconds = std::chrono::nanoseconds{(ticks % 10'000'000) * 100};
                const auto createdTicks =
                    (static_cast<std::uint64_t>(information.ftCreationTime.dwHighDateTime) << 32) |
                    information.ftCreationTime.dwLowDateTime;
                const auto created = std::chrono::sys_seconds{std::chrono::seconds{
                    static_cast<std::int64_t>(createdTicks / 10'000'000) - 11'644'473'600LL}};
                return {std::to_string(information.dwVolumeSerialNumber) + ':' +
                            std::to_string(index),
                        size, modified, subseconds, created};
#else
                struct stat information{};
                if (::fstat(handle_, &information) != 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "cannot inspect source handle");
                }
                if (!S_ISREG(information.st_mode)) {
                    throw std::system_error(
                        std::make_error_code(std::errc::operation_not_permitted),
                        "source is not a regular file");
                }
#ifdef __APPLE__
                const auto modified = information.st_mtimespec;
                const std::optional<std::chrono::sys_seconds> created{
                    std::chrono::seconds{information.st_birthtimespec.tv_sec}};
#else
                const auto modified = information.st_mtim;
                std::optional<std::chrono::sys_seconds> created;
#ifdef __linux__
                struct statx extended{};
                if (::statx(handle_, "", AT_EMPTY_PATH, STATX_BTIME, &extended) == 0) {
                    if ((extended.stx_mask & STATX_BTIME) != 0) {
                        created = std::chrono::sys_seconds{
                            std::chrono::seconds{extended.stx_btime.tv_sec}};
                    }
                } else if (errno != ENOSYS && errno != EOPNOTSUPP && errno != EINVAL) {
                    throw std::system_error(errno, std::generic_category(),
                                            "cannot inspect source creation time");
                }
#endif
#endif
                return {std::to_string(static_cast<std::uintmax_t>(information.st_dev)) + ':' +
                            std::to_string(static_cast<std::uintmax_t>(information.st_ino)),
                        static_cast<std::uintmax_t>(information.st_size),
                        std::chrono::sys_time<std::chrono::seconds>{
                            std::chrono::seconds{modified.tv_sec}},
                        std::chrono::nanoseconds{modified.tv_nsec}, created};
#endif
            }

            [[nodiscard]] std::size_t read(const std::span<char> buffer) const {
#ifdef _WIN32
                DWORD bytes = 0;
                if (ReadFile(handle_, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes,
                             nullptr) == 0) {
                    throw std::system_error(static_cast<int>(GetLastError()),
                                            std::system_category(), "cannot read source file");
                }
#else
                ssize_t bytes;
                do {
                    bytes = ::read(handle_, buffer.data(), buffer.size());
                } while (bytes < 0 && errno == EINTR);
                if (bytes < 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "cannot read source file");
                }
#endif
                return static_cast<std::size_t>(bytes);
            }

            void close() {
#ifdef _WIN32
                const auto handle = std::exchange(handle_, INVALID_HANDLE_VALUE);
                if (CloseHandle(handle) == 0) {
                    throw std::system_error(static_cast<int>(GetLastError()),
                                            std::system_category(), "cannot close source file");
                }
#else
                const auto handle = std::exchange(handle_, -1);
                if (::close(handle) != 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "cannot close source file");
                }
#endif
            }

#ifdef _WIN32
            [[nodiscard]] bool sharesWrite() const {
                return sharedWrite_;
            }

            void rewind() const {
                if (SetFilePointerEx(handle_, {}, nullptr, FILE_BEGIN) == 0) {
                    throw std::system_error(static_cast<int>(GetLastError()),
                                            std::system_category(), "cannot rewind source file");
                }
            }
#endif

          private:
#ifdef _WIN32
            HANDLE handle_ = INVALID_HANDLE_VALUE;
            bool sharedWrite_ = false;
#else
            int handle_ = -1;
#endif
        };

    } // namespace

    std::string SourceFileSnapshot::modifiedTimestamp() const {
        return QDateTime::fromSecsSinceEpoch(modified.time_since_epoch().count())
            .toLocalTime()
            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            .toStdString();
    }

    std::string SourceFileSnapshot::createdTimestamp() const {
        return created ? QDateTime::fromSecsSinceEpoch(created->time_since_epoch().count())
                             .toLocalTime()
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                             .toStdString()
                       : std::string{};
    }

    std::string SourceFileSnapshot::diagnostic() const {
        return "size=" + std::to_string(size) +
               " mtime_seconds=" + std::to_string(modified.time_since_epoch().count()) +
               " mtime_nanoseconds=" + std::to_string(modifiedSubseconds.count()) +
               " identity=" + identity;
    }

    std::filesystem::file_time_type SourceFileSnapshot::fileModificationTime() const {
#ifdef _MSC_VER
        const auto fileTime = std::chrono::clock_cast<std::chrono::file_clock>(modified);
#else
        const auto fileTime = std::chrono::file_clock::from_sys(modified);
#endif
        using FileDuration = std::filesystem::file_time_type::duration;
        constexpr auto ticksPerSecond =
            std::chrono::duration_cast<FileDuration>(std::chrono::seconds{1}).count();
        auto seconds = fileTime.time_since_epoch().count();
        auto fraction = std::chrono::duration_cast<FileDuration>(modifiedSubseconds).count();
        // Normalize negative timestamps toward zero before multiplying into native ticks.
        if (seconds < 0 && fraction > 0) {
            ++seconds;
            fraction -= ticksPerSecond;
        }
        constexpr auto minimum = (FileDuration::min)().count();
        constexpr auto maximum = (FileDuration::max)().count();
        if (seconds < minimum / ticksPerSecond || seconds > maximum / ticksPerSecond) {
            throw std::range_error("source mtime is outside the native filesystem clock range");
        }
        const auto ticks = static_cast<FileDuration::rep>(seconds) * ticksPerSecond;
        if ((fraction < 0 && ticks < minimum - fraction) ||
            (fraction > 0 && ticks > maximum - fraction)) {
            throw std::range_error("source mtime is outside the native filesystem clock range");
        }
        return std::filesystem::file_time_type{FileDuration{ticks + fraction}};
    }

    SourceReadDeferred::SourceReadDeferred(std::string reason, const std::string& detail)
        : std::runtime_error(reason + " " + detail), reason_(std::move(reason)) {}

    const std::string& SourceReadDeferred::reason() const noexcept {
        return reason_;
    }

    SourceFileSnapshot captureSourceFile(const std::filesystem::path& path,
                                         const std::function<void(std::span<const char>)>& consume,
                                         const std::stop_token stopToken,
                                         const std::uintmax_t maxBytes,
                                         const std::function<void()>& afterFirstChunkRead) {
        const auto checkCanceled = [&] {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "source capture canceled");
            }
        };
        checkCanceled();
        SourceHandle source{path, false};
        const auto before = source.snapshot();
        if (before.size > maxBytes) {
            throw std::runtime_error("source file too large max_bytes=" + std::to_string(maxBytes));
        }
        constexpr std::size_t kReadBlockBytes = 1024 * 1024;
        std::vector<char> buffer(static_cast<std::size_t>(
            (std::min)(before.size, static_cast<std::uintmax_t>(kReadBlockBytes))));
        std::uintmax_t copied = 0;
#ifdef _WIN32
        std::vector<char> firstCopy;
        if (source.sharesWrite()) {
            firstCopy.reserve(static_cast<std::size_t>(before.size));
        }
#endif
        while (copied < before.size) {
            checkCanceled();
            const auto requested = static_cast<std::size_t>(
                (std::min)(before.size - copied, static_cast<std::uintmax_t>(buffer.size())));
            const auto bytes = source.read(std::span<char>{buffer}.first(requested));
            if (bytes == 0) {
                break;
            }
            consume(std::span<const char>{buffer.data(), bytes});
#ifdef _WIN32
            if (source.sharesWrite()) {
                firstCopy.insert(firstCopy.end(), buffer.data(), buffer.data() + bytes);
            }
#endif
            const bool firstChunk = copied == 0;
            copied += bytes;
            if (firstChunk && afterFirstChunkRead) {
                afterFirstChunkRead();
            }
        }
        checkCanceled();
        bool identical = true;
#ifdef _WIN32
        if (source.sharesWrite()) {
            source.rewind();
            std::size_t verified = 0;
            while (verified < firstCopy.size()) {
                checkCanceled();
                const auto requested = (std::min)(firstCopy.size() - verified, buffer.size());
                const auto bytes = source.read(std::span<char>{buffer}.first(requested));
                if (bytes == 0) {
                    identical = false;
                    break;
                }
                identical =
                    std::equal(buffer.data(), buffer.data() + bytes, firstCopy.data() + verified) &&
                    identical;
                verified += bytes;
            }
        }
#endif
        checkCanceled();
        const auto after = source.snapshot();
        if (before != after || copied != before.size || !identical) {
            throw SourceReadDeferred{"source_changed_during_read",
                                     "before={" + before.diagnostic() + "} after={" +
                                         after.diagnostic() + "} copied=" + std::to_string(copied) +
                                         " copies_identical=" + (identical ? "true" : "false")};
        }
        source.close();
        checkCanceled();
        return before;
    }

    std::optional<SourceFileSnapshot> inspectSourceFileIdentity(const std::filesystem::path& path,
                                                                std::error_code& error) {
        try {
            SourceHandle source{path, true};
            const auto snapshot = source.snapshot();
            source.close();
            error.clear();
            return snapshot;
        } catch (const std::system_error& exception) {
            error = exception.code();
            return std::nullopt;
        }
    }

} // namespace ssa::infra::importing
