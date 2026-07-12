#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <optional>

namespace ssa::theme_lab {

    struct Base16Scheme {
        QString slug;
        bool dark{false};
        std::array<QColor, 16> colors;
    };

    struct SemanticPalette {
        QString slug;
        bool dark{false};
        QMap<QString, QColor> roles;
        double score{0.0};
    };

    [[nodiscard]] std::optional<Base16Scheme> parseBase16Scheme(const QJsonObject& object,
                                                                QStringList& errors);
    [[nodiscard]] SemanticPalette buildSemanticPalette(const Base16Scheme& scheme);
    [[nodiscard]] QStringList validateSemanticPalette(const SemanticPalette& palette);
    [[nodiscard]] double paletteDistance(const SemanticPalette& first,
                                         const SemanticPalette& second);
    [[nodiscard]] QVector<SemanticPalette> rankAndDeduplicate(QVector<SemanticPalette> palettes,
                                                              qsizetype limit,
                                                              double minimumDistance = 0.035);
    [[nodiscard]] QJsonObject serializePalette(const SemanticPalette& palette);
    [[nodiscard]] QVector<Base16Scheme> readCatalog(const QString& inputDirectory,
                                                    QStringList& errors);
    [[nodiscard]] QJsonObject analyzeCatalog(const QVector<Base16Scheme>& schemes, qsizetype limit);

} // namespace ssa::theme_lab
