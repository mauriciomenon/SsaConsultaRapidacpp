#include "platform/SamUrlBuilder.h"

#include <QUrlQuery>

namespace ssa::platform {

    namespace {

        QString publicViewPath(const QString& basePath) {
            QString path = basePath;
            if (path.isEmpty()) {
                path = "/";
            }
            if (path.endsWith("/SSAPublicView.aspx")) {
                return path;
            }
            if (!path.endsWith('/')) {
                path.append('/');
            }
            return path == "/" ? QStringLiteral("/SSAPublicView.aspx")
                               : path + QStringLiteral("SSAPublicView.aspx");
        }

    } // namespace

    QUrl SamUrlBuilder::publicSsaUrl(const QUrl& baseUrl, const std::string& ssaNumber) {
        QUrl url = baseUrl;
        url.setPath(publicViewPath(baseUrl.path()));
        QUrlQuery query{url};
        query.addQueryItem("SerialNumber", QString::fromStdString(ssaNumber));
        query.addQueryItem("language", QStringLiteral("pt"));
        url.setQuery(query);
        return url;
    }

} // namespace ssa::platform
