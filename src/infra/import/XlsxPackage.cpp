#include "infra/import/XlsxPackage.h"

#include "infra/import/SourceFileCapture.h"

#include <miniz.h>

#include <exception>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace ssa::infra::importing {

    class XlsxPackage::Storage final {
      public:
        explicit Storage(const std::filesystem::path& path, const std::stop_token& stopToken,
                         const std::function<void()>& afterFirstChunkRead) {
            // Capture once before parsing any sheet; miniz never observes the live source.
            snapshot_ = captureSourceFile(
                path,
                [this](const std::span<const char> chunk) {
                    bytes_.insert(bytes_.end(), chunk.begin(), chunk.end());
                },
                stopToken, 128ULL * 1024ULL * 1024ULL, afterFirstChunkRead);
            if (snapshot_.size == 0) {
                throw std::runtime_error("xlsx file is empty");
            }
            if (mz_zip_reader_init_mem(&archive_, bytes_.data(), bytes_.size(), 0) == 0) {
                throw std::runtime_error("cannot read xlsx zip package");
            }
            open_ = true;
        }

        ~Storage() {
            if (open_) {
                mz_zip_reader_end(&archive_);
            }
        }

        Storage(const Storage&) = delete;
        Storage& operator=(const Storage&) = delete;

        [[nodiscard]] const SourceFileSnapshot& sourceSnapshot() const {
            return snapshot_;
        }

        [[nodiscard]] std::string textEntry(const std::string& entryName, const bool required,
                                            const std::stop_token& stopToken) {
            constexpr mz_uint64 kMaxXmlEntryBytes = 32ULL * 1024ULL * 1024ULL;
            std::string text;
            extractEntry(
                entryName, required, kMaxXmlEntryBytes,
                [&](const std::string_view chunk) { text.append(chunk); }, stopToken);
            return text;
        }

        void streamTextEntry(const std::string& entryName, const bool required,
                             const XlsxPackage::EntryChunkConsumer& consume,
                             const std::stop_token& stopToken) {
            constexpr mz_uint64 kMaxStreamedXmlEntryBytes = 96ULL * 1024ULL * 1024ULL;
            extractEntry(entryName, required, kMaxStreamedXmlEntryBytes, consume, stopToken);
        }

      private:
        void extractEntry(const std::string& entryName, const bool required,
                          const mz_uint64 maxEntryBytes,
                          const XlsxPackage::EntryChunkConsumer& consume,
                          const std::stop_token& stopToken) {
            constexpr mz_uint64 kMaxTotalXmlBytes = 96ULL * 1024ULL * 1024ULL;
            const int fileIndex =
                mz_zip_reader_locate_file(&archive_, entryName.c_str(), nullptr, 0);
            if (fileIndex < 0) {
                if (required) {
                    throw std::runtime_error("missing xlsx entry: " + entryName);
                }
                return;
            }
            mz_zip_archive_file_stat stat{};
            if (mz_zip_reader_file_stat(&archive_, static_cast<mz_uint>(fileIndex), &stat) == 0) {
                throw std::runtime_error("cannot stat xlsx entry: " + entryName);
            }
            if (stat.m_uncomp_size > maxEntryBytes) {
                throw std::runtime_error("xlsx entry too large: " + entryName);
            }
            if (extractedBytes_ + stat.m_uncomp_size > kMaxTotalXmlBytes) {
                throw std::runtime_error("xlsx package XML payload too large");
            }
            struct ExtractionContext {
                const XlsxPackage::EntryChunkConsumer* consume = nullptr;
                std::stop_token stopToken;
                mz_uint64 extractedBytes = 0;
                std::exception_ptr exception;
                bool canceled = false;
                bool failed = false;
            } context{&consume, stopToken};
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            const auto appendChunk = [](void* opaque, const mz_uint64 offset, const void* data,
                                        const size_t size) -> size_t {
                auto& extraction = *static_cast<ExtractionContext*>(opaque);
                if (extraction.stopToken.stop_requested()) {
                    extraction.canceled = true;
                    return 0;
                }
                if (offset != extraction.extractedBytes) {
                    extraction.failed = true;
                    return 0;
                }
                try {
                    (*extraction.consume)(
                        {static_cast<const char*>(data), static_cast<std::size_t>(size)});
                } catch (...) {
                    extraction.exception = std::current_exception();
                    return 0;
                }
                extraction.extractedBytes += size;
                return size;
            };
            const auto extracted = mz_zip_reader_extract_to_callback(
                &archive_, static_cast<mz_uint>(fileIndex), appendChunk, &context, 0);
            if (context.canceled || stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            if (context.exception) {
                std::rethrow_exception(context.exception);
            }
            if (extracted == 0 || context.failed) {
                throw std::runtime_error("cannot extract xlsx entry: " + entryName);
            }
            extractedBytes_ += context.extractedBytes;
        }
        std::vector<char> bytes_;
        SourceFileSnapshot snapshot_;
        mz_zip_archive archive_{};
        mz_uint64 extractedBytes_{0};
        bool open_{false};
    };

    XlsxPackage::XlsxPackage(const std::filesystem::path& path, const std::stop_token& stopToken,
                             const std::function<void()>& afterFirstChunkRead)
        : storage_(std::make_unique<Storage>(path, stopToken, afterFirstChunkRead)) {}

    XlsxPackage::~XlsxPackage() = default;

    const SourceFileSnapshot& XlsxPackage::sourceSnapshot() const {
        return storage_->sourceSnapshot();
    }

    std::string XlsxPackage::textEntry(const std::string& entryName, const bool required,
                                       const std::stop_token& stopToken) {
        return storage_->textEntry(entryName, required, stopToken);
    }

    void XlsxPackage::streamTextEntry(const std::string& entryName, const bool required,
                                      const EntryChunkConsumer& consume,
                                      const std::stop_token& stopToken) {
        storage_->streamTextEntry(entryName, required, consume, stopToken);
    }

} // namespace ssa::infra::importing
