#pragma once

#include <QString>

#include <filesystem>

namespace ssa::platform {

    class AppPaths final {
      public:
        AppPaths(QString projectRoot, QString configDir);

        [[nodiscard]] QString preferencesFile() const;
        [[nodiscard]] std::filesystem::path projectRootPath() const;
        [[nodiscard]] std::filesystem::path configDirectoryPath() const;
        [[nodiscard]] std::filesystem::path inputFolderPath() const;
        [[nodiscard]] std::filesystem::path processedFolderPath() const;
        [[nodiscard]] std::filesystem::path redundantFolderPath() const;
        // README.md of the project root (a development clone) wins; an installed package ships
        // it in a documents-only folder beside the executable (Contents/Resources/doc in a
        // macOS bundle), as the Python GUI also looks in its bundled root.
        [[nodiscard]] std::filesystem::path
        installationGuidePath(const std::filesystem::path& applicationDirectory) const;
        void ensureConfigDirectory() const;
        void ensureDataDirectory() const;
        void ensureInputFolders() const;

      private:
        QString projectRoot_;
        QString configDir_;
    };

} // namespace ssa::platform
