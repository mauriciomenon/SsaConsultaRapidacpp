#include "platform/DesktopExternalCommandPort.h"

#include <QDesktopServices>
#include <QDir>
#include <QUrlQuery>

namespace ssa::platform {

    DesktopExternalCommandPort::DesktopExternalCommandPort(QUrl samBaseUrl)
        : samBaseUrl_(std::move(samBaseUrl)) {}

    void DesktopExternalCommandPort::openSamHome() {
        QDesktopServices::openUrl(samBaseUrl_);
    }

    void DesktopExternalCommandPort::openSsa(const std::string& numeroSsa) {
        QUrl url = samBaseUrl_;
        url.setPath("/ssa");
        QUrlQuery query;
        query.addQueryItem("numero", QString::fromStdString(numeroSsa));
        url.setQuery(query);
        QDesktopServices::openUrl(url);
    }

    void DesktopExternalCommandPort::openPath(const std::string& path) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
    }

    void DesktopExternalCommandPort::exportSelection(
        const std::vector<std::map<std::string, std::string>>& rows) {
        Q_UNUSED(rows);
    }

    void DesktopExternalCommandPort::requestCommand(
        const std::string& command, const std::map<std::string, std::string>& parameters) {
        if (command == "open_sam") {
            const auto it = parameters.find("numero_ssa");
            if (it == parameters.end()) {
                openSamHome();
                return;
            }
            openSsa(it->second);
        }
    }

} // namespace ssa::platform
