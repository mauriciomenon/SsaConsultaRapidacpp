#include "infra/import/XlsxPackage.h"

#include <QFile>
#include <QString>

#include <miniz.h>

#include <cstring>
#include <memory>
#include <stdexcept>

namespace ssa::infra::importing {

    namespace {

        QString pathToQString(const std::filesystem::path& path) {
#ifdef _WIN32
            return QString::fromStdWString(path.wstring());
#else
            const auto native = path.native();
            return QFile::decodeName(native.data());
#endif
        }

    } // namespace

    class XlsxPackage::Storage final {
      public:
        explicit Storage(const std::filesystem::path& path) : file_(pathToQString(path)) {
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

        [[nodiscard]] std::string textEntry(const std::string& entryName, const bool required) {
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
            size_t size = 0;
            using ZipHeapPtr = std::unique_ptr<void, decltype(&mz_free)>;
            ZipHeapPtr data{
                mz_zip_reader_extract_to_heap(&archive_, static_cast<mz_uint>(fileIndex), &size, 0),
                &mz_free};
            if (data == nullptr) {
                throw std::runtime_error("cannot extract xlsx entry: " + entryName);
            }
            std::string text{static_cast<const char*>(data.get()), size};
            extractedBytes_ += static_cast<mz_uint64>(size);
            return text;
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

    std::string XlsxPackage::textEntry(const std::string& entryName, const bool required) {
        return storage_->textEntry(entryName, required);
    }

} // namespace ssa::infra::importing
