#include "platform/OpenPathPolicy.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#ifdef _WIN32
#include <wchar.h>
#endif

namespace ssa::platform {

    namespace {

        bool containsControlCharacter(const std::string& value) {
            return std::ranges::any_of(value, [](const char ch) {
                return std::iscntrl(static_cast<unsigned char>(ch)) != 0;
            });
        }

        bool samePathComponent(const std::filesystem::path& left,
                               const std::filesystem::path& right) {
#ifdef _WIN32
            const auto& leftValue = left.native();
            const auto& rightValue = right.native();
            return _wcsicmp(leftValue.c_str(), rightValue.c_str()) == 0;
#else
            return left == right;
#endif
        }

        bool sameNativePrefix(const std::filesystem::path::string_type& path,
                              const std::filesystem::path::string_type& root) {
            if (path.size() < root.size()) {
                return false;
            }
#ifdef _WIN32
            return _wcsnicmp(path.c_str(), root.c_str(), root.size()) == 0;
#else
            return path.compare(0, root.size(), root) == 0;
#endif
        }

        bool isNativeSeparator(const std::filesystem::path::value_type value) {
            return value == std::filesystem::path::preferred_separator
#ifdef _WIN32
                   || value == L'/' || value == L'\\'
#endif
                ;
        }

        bool samePathRoot(const std::filesystem::path& path, const std::filesystem::path& root) {
            return samePathComponent(path.root_name(), root.root_name()) &&
                   samePathComponent(path.root_directory(), root.root_directory());
        }

        bool pathIsWithinRoot(const std::filesystem::path& path,
                              const std::filesystem::path& root) {
            if (!samePathRoot(path, root)) {
                return false;
            }

            const auto& pathValue = path.native();
            const auto& rootValue = root.native();
            if (!sameNativePrefix(pathValue, rootValue)) {
                return false;
            }
            return pathValue.size() == rootValue.size() ||
                   isNativeSeparator(pathValue[rootValue.size()]);
        }

    } // namespace

    OpenPathPolicy::OpenPathPolicy(std::vector<std::filesystem::path> allowedRoots) {
        canonicalAllowedRoots_.reserve(allowedRoots.size());
        for (auto& root : allowedRoots) {
            if (!root.is_absolute()) {
                ++rejectedAllowedRoots_;
                continue;
            }
            auto canonicalRoot = canonicalizePath(root);
            if (canonicalRoot && !canonicalRoot->empty() &&
                *canonicalRoot != canonicalRoot->root_path()) {
                canonicalAllowedRoots_.push_back(std::move(*canonicalRoot));
            } else {
                ++rejectedAllowedRoots_;
            }
        }
        canonicalAllowedRoots_.shrink_to_fit();
    }

    std::optional<std::filesystem::path>
    OpenPathPolicy::canonicalizePath(const std::filesystem::path& path) {
        std::error_code error;
        auto normalized = std::filesystem::weakly_canonical(path, error);
        if (!error) {
            return normalized;
        }
        return std::nullopt;
    }

    ports::ExternalCommandResult OpenPathPolicy::validate(const std::string& rawPath) const {
        if (rawPath.empty()) {
            return {ports::ExternalCommandStatus::Rejected, "path is empty"};
        }
        if (containsControlCharacter(rawPath)) {
            return {ports::ExternalCommandStatus::Rejected,
                    "path contains illegal control characters"};
        }
        const std::filesystem::path raw{rawPath};
        if (!raw.is_absolute()) {
            return {ports::ExternalCommandStatus::Rejected, "path must be absolute"};
        }
        const auto path = canonicalizePath(raw);
        if (!path) {
            return {ports::ExternalCommandStatus::Rejected, "path cannot be resolved"};
        }
        if (canonicalAllowedRoots_.empty()) {
            if (rejectedAllowedRoots_ > 0) {
                return {ports::ExternalCommandStatus::Rejected,
                        "open path policy has no valid allowed roots"};
            }
            return {ports::ExternalCommandStatus::Rejected,
                    "open path policy has no allowed roots"};
        }
        const bool allowed = std::ranges::any_of(canonicalAllowedRoots_, [&path](const auto& root) {
            return pathIsWithinRoot(*path, root);
        });
        if (!allowed) {
            return {ports::ExternalCommandStatus::Rejected, "path is outside allowed roots"};
        }
        return {ports::ExternalCommandStatus::Succeeded, {}};
    }

} // namespace ssa::platform
