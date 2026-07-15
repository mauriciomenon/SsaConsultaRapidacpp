#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>

namespace ssa::infra::importing {

    class XlsxPackage final {
      public:
        using EntryChunkConsumer = std::function<void(std::string_view)>;

        explicit XlsxPackage(const std::filesystem::path& path);
        ~XlsxPackage();

        XlsxPackage(const XlsxPackage&) = delete;
        XlsxPackage& operator=(const XlsxPackage&) = delete;
        XlsxPackage(XlsxPackage&&) = delete;
        XlsxPackage& operator=(XlsxPackage&&) = delete;

        [[nodiscard]] std::string textEntry(const std::string& entryName, bool required,
                                            const std::stop_token& stopToken = {});
        void streamTextEntry(const std::string& entryName, bool required,
                             const EntryChunkConsumer& consume,
                             const std::stop_token& stopToken = {});

      private:
        class Storage;
        std::unique_ptr<Storage> storage_;
    };

} // namespace ssa::infra::importing
