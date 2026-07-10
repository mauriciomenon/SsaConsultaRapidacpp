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
        [[nodiscard]] int version() const;
        [[nodiscard]] QStringList optionsFor(const QString& key) const;
        [[nodiscard]] bool loadingFor(const QString& key) const;
        [[nodiscard]] bool hasFreshOptions(const QString& key, std::uint64_t stateVersion) const;

        void clearLoading();
        void markLoading(const QString& key);
        void store(const std::vector<std::string>& options, const QString& key,
                   std::uint64_t stateVersion);
        void touchVersion();

      private:
        struct CacheEntry final {
            std::vector<std::string> source;
            QStringList options;
            std::uint64_t stateVersion{0};
        };

        [[nodiscard]] std::map<QString, CacheEntry>::const_iterator
        findTrimmed(const QString& key) const;
        void trim(const QString& protectedKey);

        std::map<QString, CacheEntry> cache_;
        QSet<QString> loadingKeys_;
        int version_{0};
    };

} // namespace ssa::presentation
