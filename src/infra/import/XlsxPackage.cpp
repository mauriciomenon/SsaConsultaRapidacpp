#include "infra/import/XlsxPackage.h"

#include "qt/FilesystemPath.h"

#include <QFile>
#include <QString>

#include <miniz.h>

#include <cstring>
#include <stdexcept>
#include <system_error>

namespace ssa::infra::importing {

    class XlsxPackage::Storage final {
      public:
        explicit Storage(const std::filesystem::path& path) : file_(qt::toQString(path)) {
            if (!file_.open(QIODevice::ReadOnly)) {
                throw std::runtime_error("cannot open xlsx file");
            }
            const auto size = file_.size();
            if (size <= 0) {
                throw std::runtime_error("xlsx file is empty");
            }
            mapped_ = file_.map(0, static_cast<qint64>(size));
            if (mapped_ == nullptr) {
                throw std::runtime_error("cannot map xlsx file");
            }
            std::memset(&archive_, 0, sizeof(archive_));
            if (mz_zip_reader_init_mem(&archive_, mapped_, static_cast<size_t>(size), 0) == 0) {
                throw std::runtime_error("cannot read xlsx zip package");
            }
            open_ = true;
        }

        ~Storage() {
            if (open_) {
                mz_zip_reader_end(&archive_);
            }
            if (mapped_ != nullptr) {
                file_.unmap(mapped_);
            }
        }

        Storage(const Storage&) = delete;
        Storage& operator=(const Storage&) = delete;

        [[nodiscard]] std::string textEntry(const std::string& entryName, const bool required,
                                            const std::stop_token stopToken) {
            constexpr mz_uint64 kMaxXmlEntryBytes = 32ULL * 1024ULL * 1024ULL;
            constexpr mz_uint64 kMaxTotalXmlBytes = 96ULL * 1024ULL * 1024ULL;
            const int fileIndex =
                mz_zip_reader_locate_file(&archive_, entryName.c_str(), nullptr, 0);
            if (fileIndex < 0) {
                if (required) {
                    throw std::runtime_error("missing xlsx entry: " + entryName);
                }
                return {};
            }
            mz_zip_archive_file_stat stat{};
            if (mz_zip_reader_file_stat(&archive_, static_cast<mz_uint>(fileIndex), &stat) == 0) {
                throw std::runtime_error("cannot stat xlsx entry: " + entryName);
            }
            if (stat.m_uncomp_size > kMaxXmlEntryBytes) {
                throw std::runtime_error("xlsx entry too large: " + entryName);
            }
            if (extractedBytes_ + stat.m_uncomp_size > kMaxTotalXmlBytes) {
                throw std::runtime_error("xlsx package XML payload too large");
            }
            struct ExtractionContext {
                std::string text;
                std::stop_token stopToken;
                bool canceled = false;
                bool failed = false;
            } context{{}, stopToken};
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            context.text.reserve(static_cast<std::size_t>(stat.m_uncomp_size));
            const auto appendChunk = [](void* opaque, const mz_uint64 offset, const void* data,
                                        const size_t size) -> size_t {
                auto& extraction = *static_cast<ExtractionContext*>(opaque);
                if (extraction.stopToken.stop_requested()) {
                    extraction.canceled = true;
                    return 0;
                }
                if (offset != extraction.text.size()) {
                    extraction.failed = true;
                    return 0;
                }
                try {
                    extraction.text.append(static_cast<const char*>(data), size);
                } catch (...) {
                    extraction.failed = true;
                    return 0;
                }
                return size;
            };
            const auto extracted = mz_zip_reader_extract_to_callback(
                &archive_, static_cast<mz_uint>(fileIndex), appendChunk, &context, 0);
            if (context.canceled || stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            if (extracted == 0 || context.failed) {
                throw std::runtime_error("cannot extract xlsx entry: " + entryName);
            }
            extractedBytes_ += static_cast<mz_uint64>(context.text.size());
            return std::move(context.text);
        }

      private:
        QFile file_;
        uchar* mapped_{nullptr};
        mz_zip_archive archive_{};
        mz_uint64 extractedBytes_{0};
        bool open_{false};
    };

    XlsxPackage::XlsxPackage(const std::filesystem::path& path)
        : storage_(std::make_unique<Storage>(path)) {}

    XlsxPackage::~XlsxPackage() = default;

    std::string XlsxPackage::textEntry(const std::string& entryName, const bool required,
                                       const std::stop_token stopToken) {
        return storage_->textEntry(entryName, required, stopToken);
    }

} // namespace ssa::infra::importing
