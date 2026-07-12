#include "presentation/FilterPanelColumnValueOptions.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ssa::presentation {

    namespace {

        constexpr std::size_t kMaxColumnValueOptionCacheEntries = 24;

        QStringList toColumnValueDisplayList(const std::vector<std::string>& orderedValues) {
            QStringList displayValues;
            displayValues.reserve(static_cast<int>(orderedValues.size()));
            for (const auto& value : orderedValues) {
                displayValues.append(QString::fromStdString(value));
            }
            return displayValues;
        }

    } // namespace

    int FilterPanelColumnValueOptions::version() const {
        return version_;
    }

    QStringList FilterPanelColumnValueOptions::optionsFor(const QString& key) const {
        const auto found = findTrimmed(key);
        return found == cache_.end() ? QStringList{} : found->second.options;
    }

    std::size_t FilterPanelColumnValueOptions::maxValueLengthFor(const QString& key) const {
        const auto found = findTrimmed(key);
        return found == cache_.end() ? 0 : found->second.maxValueLength;
    }

    bool FilterPanelColumnValueOptions::loadingFor(const QString& key) const {
        return loadingKeys_.contains(key.trimmed());
    }

    bool FilterPanelColumnValueOptions::hasFreshOptions(const QString& key,
                                                        const std::uint64_t stateVersion) const {
        const auto found = findTrimmed(key);
        return found != cache_.end() && found->second.stateVersion == stateVersion;
    }

    void FilterPanelColumnValueOptions::clearLoading() {
        loadingKeys_.clear();
    }

    void FilterPanelColumnValueOptions::clearLoadingFor(const QString& key) {
        if (loadingKeys_.remove(key.trimmed())) {
            touchVersion();
        }
    }

    void FilterPanelColumnValueOptions::markLoading(const QString& key) {
        loadingKeys_.insert(key.trimmed());
        touchVersion();
    }

    void FilterPanelColumnValueOptions::store(const std::vector<std::string>& options,
                                              const QString& key, const std::uint64_t stateVersion,
                                              const std::size_t maxValueLength) {
        const auto normalizedKey = key.trimmed();
        loadingKeys_.remove(normalizedKey);
        auto displayList = toColumnValueDisplayList(options);
        cache_[normalizedKey] = {std::move(displayList), stateVersion, maxValueLength};
        trim(normalizedKey);
        touchVersion();
    }

    void FilterPanelColumnValueOptions::touchVersion() {
        ++version_;
    }

    std::map<QString, FilterPanelColumnValueOptions::CacheEntry>::const_iterator
    FilterPanelColumnValueOptions::findTrimmed(const QString& key) const {
        return cache_.find(key.trimmed());
    }

    void FilterPanelColumnValueOptions::trim(const QString& protectedKey) {
        while (cache_.size() > kMaxColumnValueOptionCacheEntries) {
            auto option = cache_.begin();
            if (option != cache_.end() && option->first == protectedKey) {
                ++option;
            }
            if (option == cache_.end()) {
                return;
            }
            cache_.erase(option);
        }
    }

} // namespace ssa::presentation
