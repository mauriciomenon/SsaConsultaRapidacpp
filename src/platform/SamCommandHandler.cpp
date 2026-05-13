#include "platform/SamCommandHandler.h"

#include <QDesktopServices>
#include <QUrlQuery>

namespace ssa::platform {

    namespace {

        QString joinedSamPath(const QString& basePath) {
            if (basePath.isEmpty() || basePath == "/") {
                return "/ssa";
            }
            if (basePath == "/ssa" || basePath.endsWith("/ssa")) {
                return basePath;
            }
            return basePath.endsWith('/') ? basePath + "ssa" : basePath + "/ssa";
        }

    } // namespace

    SamCommandHandler::SamCommandHandler(QUrl samBaseUrl) : samBaseUrl_(std::move(samBaseUrl)) {}

    ports::ExternalCommandResult SamCommandHandler::openHome() const {
        if (!QDesktopServices::openUrl(samBaseUrl_)) {
            return {ports::ExternalCommandStatus::Failed, "failed to open SAM home"};
        }
        return {ports::ExternalCommandStatus::Succeeded, "SAM home opened"};
    }

    ports::ExternalCommandResult SamCommandHandler::openSsa(const std::string& ssaNumber) const {
        QUrl url = samBaseUrl_;
        url.setPath(joinedSamPath(samBaseUrl_.path()));
        QUrlQuery query{url};
        query.addQueryItem("numero", QString::fromStdString(ssaNumber));
        url.setQuery(query);
        if (!QDesktopServices::openUrl(url)) {
            return {ports::ExternalCommandStatus::Failed, "failed to open SAM SSA"};
        }
        return {ports::ExternalCommandStatus::Succeeded, "SAM SSA opened"};
    }

} // namespace ssa::platform
