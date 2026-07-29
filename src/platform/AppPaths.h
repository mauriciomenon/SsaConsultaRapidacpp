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
        [[nodiscard]] std::filesystem::path installationGuidePath() const;
        void ensureConfigDirectory() const;
        void ensureDataDirectory() const;
        void ensureInputFolders() const;

      private:
        QString projectRoot_;
        QString configDir_;
    };

} // namespace ssa::platform
