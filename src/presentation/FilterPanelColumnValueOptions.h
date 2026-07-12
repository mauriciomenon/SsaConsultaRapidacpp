#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ssa::presentation {

    class FilterPanelColumnValueOptions final {
      public:
        [[nodiscard]] QStringList optionsFor(const QString& key) const;
        [[nodiscard]] std::size_t maxValueLengthFor(const QString& key) const;
        [[nodiscard]] bool loadingFor(const QString& key) const;
        [[nodiscard]] QString errorFor(const QString& key) const;
        [[nodiscard]] bool hasFreshOptions(const QString& key, std::uint64_t stateVersion) const;

        void clearLoading();
        void clearLoadingFor(const QString& key);
        void markLoading(const QString& key);
        void markFailed(const QString& key, const QString& message);
        void store(const std::vector<std::string>& options, const QString& key,
                   std::uint64_t stateVersion, std::size_t maxValueLength = 0);

      private:
        struct CacheEntry final {
            QStringList options;
            std::uint64_t stateVersion{0};
            std::size_t maxValueLength{0};
        };

        [[nodiscard]] std::map<QString, CacheEntry>::const_iterator
        findTrimmed(const QString& key) const;
        void trim(const QString& protectedKey);

        std::map<QString, CacheEntry> cache_;
        QSet<QString> loadingKeys_;
        std::map<QString, QString> errors_;
    };

} // namespace ssa::presentation
