#include "platform/SamCommandHandler.h"

#include <QDesktopServices>

#include <algorithm>
#include <cctype>
#include <string_view>

#include "platform/SamUrlBuilder.h"

namespace ssa::platform {

    namespace {

        bool hasConfiguredSamUrl(const QUrl& url) {
            return !url.isEmpty() && url.isValid() && !url.scheme().isEmpty();
        }

        bool isValidSsaNumber(const std::string_view value) {
            return !value.empty() && std::ranges::all_of(value, [](const unsigned char character) {
                return std::isdigit(character) != 0;
            });
        }

    } // namespace

    SamCommandHandler::SamCommandHandler(QUrl samBaseUrl) : samBaseUrl_(std::move(samBaseUrl)) {}

    ports::ExternalCommandResult SamCommandHandler::openHome() const {
        if (!hasConfiguredSamUrl(samBaseUrl_)) {
            return {ports::ExternalCommandStatus::Rejected, "SAM URL is not configured"};
        }
        if (!QDesktopServices::openUrl(samBaseUrl_)) {
            return {ports::ExternalCommandStatus::Failed, "failed to open SAM home"};
        }
        return {ports::ExternalCommandStatus::Succeeded, "SAM home opened"};
    }

    ports::ExternalCommandResult SamCommandHandler::openSsa(const std::string& ssaNumber) const {
        if (!hasConfiguredSamUrl(samBaseUrl_)) {
            return {ports::ExternalCommandStatus::Rejected, "SAM URL is not configured"};
        }
        if (!isValidSsaNumber(ssaNumber)) {
            return {ports::ExternalCommandStatus::Rejected, "ssa_number must be numeric"};
        }
        const auto url = SamUrlBuilder::publicSsaUrl(samBaseUrl_, ssaNumber);
        if (!QDesktopServices::openUrl(url)) {
            return {ports::ExternalCommandStatus::Failed, "failed to open SAM SSA"};
        }
        return {ports::ExternalCommandStatus::Succeeded, "SAM SSA opened"};
    }

} // namespace ssa::platform
