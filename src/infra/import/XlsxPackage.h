#pragma once

#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>

namespace ssa::infra::importing {

    class XlsxPackage final {
      public:
        explicit XlsxPackage(const std::filesystem::path& path);
        ~XlsxPackage();

        XlsxPackage(const XlsxPackage&) = delete;
        XlsxPackage& operator=(const XlsxPackage&) = delete;
        XlsxPackage(XlsxPackage&&) = delete;
        XlsxPackage& operator=(XlsxPackage&&) = delete;

        [[nodiscard]] std::string textEntry(const std::string& entryName, bool required,
                                            const std::stop_token& stopToken = {});

      private:
        class Storage;
        std::unique_ptr<Storage> storage_;
    };

} // namespace ssa::infra::importing
