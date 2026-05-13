#include "platform/OpenPathPolicy.h"

#include <QString>
#include <Qt>

#include <algorithm>
#include <cctype>

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
            return QString::fromStdWString(left.wstring())
                       .compare(QString::fromStdWString(right.wstring()), Qt::CaseInsensitive) == 0;
#else
            return left == right;
#endif
        }

        bool pathIsWithinRoot(const std::filesystem::path& path,
                              const std::filesystem::path& root) {
            std::error_code error;
            const auto relative = std::filesystem::relative(path, root, error);
            if (error) {
                return false;
            }
            if (relative.empty()) {
                return true;
            }
            for (const auto& part : relative) {
#ifdef _WIN32
                if (samePathComponent(part, "..")) {
#else
                if (part == "..") {
#endif
                    return false;
                }
            }
            return true;
        }

    } // namespace

    OpenPathPolicy::OpenPathPolicy(std::vector<std::filesystem::path> allowedRoots) {
        canonicalAllowedRoots_.reserve(allowedRoots.size());
        for (auto& root : allowedRoots) {
            if (root.is_absolute()) {
                if (auto canonicalRoot = normalizePath(root)) {
                    canonicalAllowedRoots_.push_back(*canonicalRoot);
                }
            }
        }
    }

    std::optional<std::filesystem::path>
    OpenPathPolicy::normalizePath(const std::filesystem::path& path) {
        std::error_code error;
        auto normalized = std::filesystem::canonical(path, error);
        if (!error) {
            return normalized;
        }
        error.clear();
        normalized = std::filesystem::weakly_canonical(path, error);
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
        const auto path = normalizePath(raw);
        if (!path) {
            return {ports::ExternalCommandStatus::Rejected, "path cannot be resolved"};
        }
        if (canonicalAllowedRoots_.empty()) {
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
