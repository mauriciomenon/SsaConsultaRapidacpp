#include "ThemePaletteLab.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLatin1StringView>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ssa::theme_lab {
    namespace {

        constexpr double kMinimumTextContrast = 4.5;
        constexpr double kMinimumSurfaceContrast = 1.03;
        constexpr double kGeneratedTextContrast = 4.6;
        constexpr double kGeneratedSurfaceContrast = 1.04;
        constexpr double kGeneratedDarkSurfaceLuminance = 0.0215;
        constexpr double kGeneratedLightSurfaceLuminance = 0.935;
        constexpr double kMaximumAccentSaturation = 0.55;
        constexpr double kGeneratedAccentSaturation = 0.53;
        constexpr double kGeneratedInteractiveContrast = 4.6;

        constexpr std::array<QLatin1StringView, 16> kBase16Keys{
            QLatin1StringView("base00"), QLatin1StringView("base01"), QLatin1StringView("base02"),
            QLatin1StringView("base03"), QLatin1StringView("base04"), QLatin1StringView("base05"),
            QLatin1StringView("base06"), QLatin1StringView("base07"), QLatin1StringView("base08"),
            QLatin1StringView("base09"), QLatin1StringView("base0A"), QLatin1StringView("base0B"),
            QLatin1StringView("base0C"), QLatin1StringView("base0D"), QLatin1StringView("base0E"),
            QLatin1StringView("base0F"),
        };

        constexpr std::array<QLatin1StringView, 20> kRoleNames{
            QLatin1StringView("window"),     QLatin1StringView("surface"),
            QLatin1StringView("panel"),      QLatin1StringView("panelRaised"),
            QLatin1StringView("header"),     QLatin1StringView("tableHeader"),
            QLatin1StringView("border"),     QLatin1StringView("borderSoft"),
            QLatin1StringView("text"),       QLatin1StringView("mutedText"),
            QLatin1StringView("accent"),     QLatin1StringView("accentText"),
            QLatin1StringView("accentSoft"), QLatin1StringView("accentStrong"),
            QLatin1StringView("link"),       QLatin1StringView("danger"),
            QLatin1StringView("dangerSoft"), QLatin1StringView("dangerStrong"),
            QLatin1StringView("rowAlt"),     QLatin1StringView("rowSelected"),
        };

        constexpr std::array<QLatin1StringView, 8> kBackgroundRoles{
            QLatin1StringView("window"), QLatin1StringView("surface"),
            QLatin1StringView("panel"),  QLatin1StringView("panelRaised"),
            QLatin1StringView("header"), QLatin1StringView("tableHeader"),
            QLatin1StringView("rowAlt"), QLatin1StringView("rowSelected"),
        };

        constexpr std::array<QLatin1StringView, 5> kInteractiveBackgroundRoles{
            QLatin1StringView("accent"),       QLatin1StringView("accentSoft"),
            QLatin1StringView("dangerSoft"),   QLatin1StringView("danger"),
            QLatin1StringView("dangerStrong"),
        };

        double linearChannel(const double channel) {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        }

        double encodedChannel(const double channel) {
            return channel <= 0.0031308 ? channel * 12.92
                                        : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
        }

        double luminance(const QColor& color) {
            return 0.2126 * linearChannel(color.redF()) + 0.7152 * linearChannel(color.greenF()) +
                   0.0722 * linearChannel(color.blueF());
        }

        double contrast(const QColor& first, const QColor& second) {
            const double lighter = std::max(luminance(first), luminance(second));
            const double darker = std::min(luminance(first), luminance(second));
            return (lighter + 0.05) / (darker + 0.05);
        }

        QColor mixLinear(const QColor& first, const QColor& second, const double amount) {
            const double bounded = std::clamp(amount, 0.0, 1.0);
            const auto mix = [bounded](const double firstChannel,
                                       const double secondChannel) -> float {
                const double linear = linearChannel(firstChannel) * (1.0 - bounded) +
                                      linearChannel(secondChannel) * bounded;
                return static_cast<float>(std::clamp(encodedChannel(linear), 0.0, 1.0));
            };
            return QColor::fromRgbF(mix(first.redF(), second.redF()),
                                    mix(first.greenF(), second.greenF()),
                                    mix(first.blueF(), second.blueF()));
        }

        QColor clampSaturation(const QColor& color, const double maximum) {
            QColor hsl = color.toHsl();
            if (hsl.hslSaturationF() > maximum) {
                hsl.setHslF(hsl.hslHueF(), static_cast<float>(maximum), hsl.lightnessF());
            }
            return hsl.toRgb();
        }

        QColor clampSurface(QColor color, const bool dark) {
            const QColor midpoint(QStringLiteral("#7f7f7f"));
            for (int step = 0; step < 100; ++step) {
                const double value = luminance(color);
                if ((dark && value >= kGeneratedDarkSurfaceLuminance) ||
                    (!dark && value <= kGeneratedLightSurfaceLuminance)) {
                    return color;
                }
                color = mixLinear(color, midpoint, 0.03);
            }
            return color;
        }

        QColor ensureContrast(QColor color, const QVector<QColor>& backgrounds,
                              const double minimum) {
            const auto passes = [&backgrounds, minimum](const QColor& candidate) {
                return std::ranges::all_of(backgrounds,
                                           [&candidate, minimum](const QColor& background) {
                                               return contrast(candidate, background) >= minimum;
                                           });
            };
            if (passes(color)) {
                return color;
            }

            double averageLuminance = 0.0;
            for (const QColor& background : backgrounds) {
                averageLuminance += luminance(background);
            }
            averageLuminance /= static_cast<double>(backgrounds.size());
            const QColor target = averageLuminance < 0.35 ? QColor(QStringLiteral("#f7f7f7"))
                                                          : QColor(QStringLiteral("#111214"));
            for (int step = 1; step <= 100; ++step) {
                const QColor adjusted = mixLinear(color, target, static_cast<double>(step) / 100.0);
                if (passes(adjusted)) {
                    return adjusted;
                }
            }
            return target;
        }

        QColor bestForeground(const QColor& background) {
            const std::array<QColor, 4> candidates{
                QColor(QStringLiteral("#15191f")), QColor(QStringLiteral("#f4f3ef")),
                QColor(QStringLiteral("#000000")), QColor(QStringLiteral("#ffffff"))};
            return *std::ranges::max_element(
                candidates, [&background](const QColor& first, const QColor& second) {
                    return contrast(first, background) < contrast(second, background);
                });
        }

        QColor ensureReadableBackground(QColor color, const QVector<QColor>& foregrounds,
                                        const QColor& surface) {
            const auto readable = [&foregrounds](const QColor& background) {
                return std::ranges::any_of(foregrounds, [&background](const QColor& foreground) {
                    return contrast(foreground, background) >= kGeneratedInteractiveContrast;
                });
            };
            if (readable(color)) {
                return color;
            }
            for (int step = 1; step <= 100; ++step) {
                const QColor adjusted =
                    mixLinear(color, surface, static_cast<double>(step) / 100.0);
                if (readable(adjusted)) {
                    return adjusted;
                }
            }
            return surface;
        }

        void ensureSurfaceSeparation(const QColor& first, QColor& second, const bool dark) {
            if (contrast(first, second) >= kGeneratedSurfaceContrast) {
                return;
            }
            const QColor target =
                dark ? QColor(QStringLiteral("#8a8d91")) : QColor(QStringLiteral("#4d5054"));
            for (int step = 1; step <= 30; ++step) {
                const QColor adjusted =
                    mixLinear(second, target, static_cast<double>(step) / 100.0);
                if (contrast(first, adjusted) >= kGeneratedSurfaceContrast) {
                    second = clampSurface(adjusted, dark);
                    return;
                }
            }
        }

        struct Oklab {
            double lightness;
            double greenRed;
            double blueYellow;
        };

        Oklab toOklab(const QColor& color) {
            const double red = linearChannel(color.redF());
            const double green = linearChannel(color.greenF());
            const double blue = linearChannel(color.blueF());
            const double l =
                std::cbrt(0.4122214708 * red + 0.5363325363 * green + 0.0514459929 * blue);
            const double m =
                std::cbrt(0.2119034982 * red + 0.6806995451 * green + 0.1073969566 * blue);
            const double s =
                std::cbrt(0.0883024619 * red + 0.2817188376 * green + 0.6299787005 * blue);
            return {
                0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
                1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
                0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
            };
        }

        double oklabDistance(const QColor& first, const QColor& second) {
            const Oklab firstLab = toOklab(first);
            const Oklab secondLab = toOklab(second);
            return std::hypot(firstLab.lightness - secondLab.lightness,
                              firstLab.greenRed - secondLab.greenRed,
                              firstLab.blueYellow - secondLab.blueYellow);
        }

        double paletteScore(const SemanticPalette& palette) {
            const QColor window = palette.roles.value(QStringLiteral("window"));
            const QColor surface = palette.roles.value(QStringLiteral("surface"));
            const QColor text = palette.roles.value(QStringLiteral("text"));
            const QColor muted = palette.roles.value(QStringLiteral("mutedText"));
            const QColor accent = palette.roles.value(QStringLiteral("accent"));
            double score = 100.0;
            score -= std::abs(contrast(window, surface) - 1.12) * 30.0;
            score -= std::max(0.0, accent.hslSaturationF() - 0.42) * 45.0;
            score -= std::abs(contrast(text, window) - 8.0) * 1.5;
            score -= std::abs(contrast(muted, window) - 5.5) * 2.0;
            return score;
        }

        QVector<QColor> backgrounds(const QMap<QString, QColor>& roles) {
            QVector<QColor> result;
            result.reserve(kBackgroundRoles.size());
            for (const QLatin1StringView role : kBackgroundRoles) {
                result.push_back(roles.value(QString(role)));
            }
            return result;
        }

    } // namespace

    std::optional<Base16Scheme> parseBase16Scheme(const QJsonObject& object, QStringList& errors) {
        errors.clear();
        const QString slug = object.value(QStringLiteral("slug")).toString();
        static const QRegularExpression slugPattern(QStringLiteral("^[a-z0-9][a-z0-9-]{0,127}$"));
        if (!slugPattern.match(slug).hasMatch()) {
            errors.push_back(QStringLiteral("invalid slug"));
        }

        const QString variant = object.value(QStringLiteral("variant")).toString();
        if (variant != QStringLiteral("dark") && variant != QStringLiteral("light")) {
            errors.push_back(QStringLiteral("invalid variant"));
        }

        const QJsonObject palette = object.value(QStringLiteral("palette")).toObject();
        std::array<QColor, 16> colors;
        static const QRegularExpression colorPattern(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
        for (std::size_t index = 0; index < kBase16Keys.size(); ++index) {
            const QLatin1StringView key = kBase16Keys[index];
            const QString value = palette.value(key).toString();
            const QColor color(value);
            if (!colorPattern.match(value).hasMatch() || !color.isValid()) {
                errors.push_back(QStringLiteral("invalid color: %1").arg(key));
                continue;
            }
            colors[index] = color;
        }

        if (!errors.isEmpty()) {
            return std::nullopt;
        }
        return Base16Scheme{slug, variant == QStringLiteral("dark"), colors};
    }

    SemanticPalette buildSemanticPalette(const Base16Scheme& scheme) {
        QColor firstSurface = clampSurface(scheme.colors[0], scheme.dark);
        QColor secondSurface = clampSurface(scheme.colors[1], scheme.dark);
        if (luminance(firstSurface) > luminance(secondSurface)) {
            std::swap(firstSurface, secondSurface);
        }

        QColor window = firstSurface;
        QColor surface = secondSurface;
        ensureSurfaceSeparation(window, surface, scheme.dark);
        QColor panel = mixLinear(window, surface, 0.55);
        QColor panelRaised = clampSurface(mixLinear(surface, scheme.colors[2], 0.55), scheme.dark);
        ensureSurfaceSeparation(panel, panelRaised, scheme.dark);
        const QColor header = mixLinear(panel, panelRaised, 0.35);
        const QColor tableHeader = mixLinear(panel, panelRaised, 0.70);
        const QColor border = clampSurface(scheme.colors[3], scheme.dark);
        const QColor borderSoft = mixLinear(panelRaised, border, 0.45);

        QColor accent = clampSaturation(scheme.colors[13], kGeneratedAccentSaturation);
        QColor danger = clampSaturation(scheme.colors[8], 0.58);
        const QColor accentText = bestForeground(accent);
        const QColor accentSoft = mixLinear(surface, accent, scheme.dark ? 0.28 : 0.20);
        const QColor accentStrong = ensureContrast(accent, {surface}, 3.0);
        const QColor link = ensureContrast(accent, {window}, kGeneratedTextContrast);
        const QColor dangerSoft = mixLinear(surface, danger, scheme.dark ? 0.24 : 0.18);
        const QColor dangerStrong = ensureContrast(danger, {surface}, 3.0);
        const QColor rowAlt = mixLinear(window, surface, 0.78);
        const QColor rowSelected = mixLinear(surface, accent, scheme.dark ? 0.24 : 0.16);

        QMap<QString, QColor> roles{
            {QStringLiteral("window"), window},
            {QStringLiteral("surface"), surface},
            {QStringLiteral("panel"), panel},
            {QStringLiteral("panelRaised"), panelRaised},
            {QStringLiteral("header"), header},
            {QStringLiteral("tableHeader"), tableHeader},
            {QStringLiteral("border"), border},
            {QStringLiteral("borderSoft"), borderSoft},
            {QStringLiteral("accent"), accent},
            {QStringLiteral("accentText"), accentText},
            {QStringLiteral("accentSoft"), accentSoft},
            {QStringLiteral("accentStrong"), accentStrong},
            {QStringLiteral("link"), link},
            {QStringLiteral("danger"), danger},
            {QStringLiteral("dangerSoft"), dangerSoft},
            {QStringLiteral("dangerStrong"), dangerStrong},
            {QStringLiteral("rowAlt"), rowAlt},
            {QStringLiteral("rowSelected"), rowSelected},
        };
        const QVector<QColor> paletteBackgrounds = backgrounds(roles);
        roles.insert(QStringLiteral("text"),
                     ensureContrast(scheme.colors[5], paletteBackgrounds, kGeneratedTextContrast));
        roles.insert(QStringLiteral("mutedText"),
                     ensureContrast(scheme.colors[4], paletteBackgrounds, kGeneratedTextContrast));
        const QVector<QColor> interactiveForegrounds{
            roles.value(QStringLiteral("text")),
            roles.value(QStringLiteral("accentText")),
            roles.value(QStringLiteral("panelRaised")),
        };
        for (const QLatin1StringView roleView : kInteractiveBackgroundRoles) {
            const QString role(roleView);
            roles.insert(
                role, ensureReadableBackground(roles.value(role), interactiveForegrounds, surface));
        }
        for (auto iterator = roles.begin(); iterator != roles.end(); ++iterator) {
            iterator.value() = QColor(iterator.value().name(QColor::HexRgb));
        }

        SemanticPalette palette{scheme.slug, scheme.dark, std::move(roles), 0.0};
        palette.score = paletteScore(palette);
        return palette;
    }

    QStringList validateSemanticPalette(const SemanticPalette& palette) {
        QStringList errors;
        for (const QLatin1StringView roleView : kRoleNames) {
            const QString role(roleView);
            if (!palette.roles.contains(role) || !palette.roles.value(role).isValid()) {
                errors.push_back(QStringLiteral("missing or invalid role: %1").arg(role));
            }
        }
        if (!errors.isEmpty()) {
            return errors;
        }

        const QColor text = palette.roles.value(QStringLiteral("text"));
        const QColor mutedText = palette.roles.value(QStringLiteral("mutedText"));
        for (const QLatin1StringView roleView : kBackgroundRoles) {
            const QString role(roleView);
            const QColor background = palette.roles.value(role);
            if (contrast(text, background) < kMinimumTextContrast) {
                errors.push_back(QStringLiteral("text contrast failed: %1").arg(role));
            }
            if (contrast(mutedText, background) < kMinimumTextContrast) {
                errors.push_back(QStringLiteral("muted contrast failed: %1").arg(role));
            }
            if (palette.dark && luminance(background) < 0.02) {
                errors.push_back(QStringLiteral("crushed dark surface: %1").arg(role));
            }
            if (!palette.dark && luminance(background) > 0.94) {
                errors.push_back(QStringLiteral("glaring light surface: %1").arg(role));
            }
        }

        const QColor accent = palette.roles.value(QStringLiteral("accent"));
        const QColor accentText = palette.roles.value(QStringLiteral("accentText"));
        if (accent.hslSaturationF() > kMaximumAccentSaturation + 0.001) {
            errors.push_back(QStringLiteral("accent saturation exceeds limit"));
        }
        if (contrast(accentText, accent) < kMinimumTextContrast) {
            errors.push_back(QStringLiteral("accent contrast failed"));
        }
        if (contrast(palette.roles.value(QStringLiteral("link")),
                     palette.roles.value(QStringLiteral("window"))) < kMinimumTextContrast) {
            errors.push_back(QStringLiteral("link contrast failed"));
        }
        const std::array<QColor, 3> interactiveForegrounds{
            text, accentText, palette.roles.value(QStringLiteral("panelRaised"))};
        for (const QLatin1StringView roleView : kInteractiveBackgroundRoles) {
            const QString role(roleView);
            const QColor background = palette.roles.value(role);
            const bool readable = std::ranges::any_of(
                interactiveForegrounds, [&background](const QColor& foreground) {
                    return contrast(foreground, background) >= kMinimumTextContrast;
                });
            if (!readable) {
                errors.push_back(QStringLiteral("interactive contrast failed: %1").arg(role));
            }
        }
        if (contrast(palette.roles.value(QStringLiteral("window")),
                     palette.roles.value(QStringLiteral("surface"))) < kMinimumSurfaceContrast) {
            errors.push_back(QStringLiteral("window and surface lack separation"));
        }
        if (palette.roles.value(QStringLiteral("panel")) ==
            palette.roles.value(QStringLiteral("panelRaised"))) {
            errors.push_back(QStringLiteral("panel depth is missing"));
        }
        return errors;
    }

    double paletteDistance(const SemanticPalette& first, const SemanticPalette& second) {
        static const std::array<QString, 5> comparisonRoles{
            QStringLiteral("window"), QStringLiteral("surface"), QStringLiteral("text"),
            QStringLiteral("accent"), QStringLiteral("danger")};
        double total = 0.0;
        for (const QString& role : comparisonRoles) {
            if (!first.roles.contains(role) || !second.roles.contains(role)) {
                return std::numeric_limits<double>::infinity();
            }
            total += oklabDistance(first.roles.value(role), second.roles.value(role));
        }
        return total / static_cast<double>(comparisonRoles.size());
    }

    QVector<SemanticPalette> rankAndDeduplicate(QVector<SemanticPalette> palettes,
                                                const qsizetype limit,
                                                const double minimumDistance) {
        std::ranges::sort(palettes,
                          [](const SemanticPalette& first, const SemanticPalette& second) {
                              if (first.score != second.score) {
                                  return first.score > second.score;
                              }
                              return first.slug < second.slug;
                          });

        QVector<SemanticPalette> result;
        for (SemanticPalette& palette : palettes) {
            if (!validateSemanticPalette(palette).isEmpty()) {
                continue;
            }
            const bool duplicate = std::ranges::any_of(
                result, [&palette, minimumDistance](const SemanticPalette& accepted) {
                    return paletteDistance(palette, accepted) < minimumDistance;
                });
            if (!duplicate) {
                result.push_back(std::move(palette));
                if (result.size() >= limit) {
                    break;
                }
            }
        }
        return result;
    }

    QJsonObject serializePalette(const SemanticPalette& palette) {
        QJsonObject roles;
        for (auto iterator = palette.roles.cbegin(); iterator != palette.roles.cend(); ++iterator) {
            roles.insert(iterator.key(), iterator.value().name(QColor::HexRgb));
        }
        return {
            {QStringLiteral("slug"), palette.slug},
            {QStringLiteral("isDark"), palette.dark},
            {QStringLiteral("score"), palette.score},
            {QStringLiteral("roles"), roles},
        };
    }

    QVector<Base16Scheme> readCatalog(const QString& inputDirectory, QStringList& errors) {
        errors.clear();
        const QDir directory(inputDirectory);
        if (!directory.exists()) {
            errors.push_back(
                QStringLiteral("input directory does not exist: %1").arg(inputDirectory));
            return {};
        }

        constexpr qint64 kMaximumSchemeFileSize = 64LL * 1024LL;
        QVector<Base16Scheme> result;
        const QStringList fileNames = directory.entryList({QStringLiteral("*.json")},
                                                          QDir::Files | QDir::Readable, QDir::Name);
        for (const QString& fileName : fileNames) {
            QFile file(directory.filePath(fileName));
            if (!file.open(QIODevice::ReadOnly)) {
                errors.push_back(QStringLiteral("%1: %2").arg(fileName, file.errorString()));
                continue;
            }
            if (file.size() > kMaximumSchemeFileSize) {
                errors.push_back(QStringLiteral("%1: file exceeds 64 KiB").arg(fileName));
                continue;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                errors.push_back(QStringLiteral("%1: invalid JSON").arg(fileName));
                continue;
            }

            QStringList schemeErrors;
            auto scheme = parseBase16Scheme(document.object(), schemeErrors);
            if (!scheme.has_value()) {
                errors.push_back(QStringLiteral("%1: %2").arg(
                    fileName, schemeErrors.join(QStringLiteral(", "))));
                continue;
            }
            result.push_back(std::move(*scheme));
        }
        return result;
    }

    QJsonObject analyzeCatalog(const QVector<Base16Scheme>& schemes, const qsizetype limit) {
        QVector<SemanticPalette> accepted;
        accepted.reserve(schemes.size());
        QJsonArray rejected;
        for (const Base16Scheme& scheme : schemes) {
            SemanticPalette palette = buildSemanticPalette(scheme);
            const QStringList validationErrors = validateSemanticPalette(palette);
            if (validationErrors.isEmpty()) {
                accepted.push_back(std::move(palette));
                continue;
            }
            QJsonArray reasons;
            for (const QString& error : validationErrors) {
                reasons.push_back(error);
            }
            rejected.push_back(QJsonObject{
                {QStringLiteral("slug"), scheme.slug},
                {QStringLiteral("reasons"), reasons},
            });
        }

        const qsizetype boundedLimit = std::max<qsizetype>(0, limit);
        const QVector<SemanticPalette> ranked = rankAndDeduplicate(accepted, accepted.size(), 0.0);
        const QVector<SemanticPalette> shortlist = rankAndDeduplicate(accepted, boundedLimit);
        QJsonArray serializedRanked;
        for (const SemanticPalette& palette : ranked) {
            serializedRanked.push_back(serializePalette(palette));
        }
        QJsonArray serializedShortlist;
        for (const SemanticPalette& palette : shortlist) {
            serializedShortlist.push_back(serializePalette(palette));
        }
        return {
            {QStringLiteral("inputCount"), schemes.size()},
            {QStringLiteral("acceptedCount"), accepted.size()},
            {QStringLiteral("rejectedCount"), rejected.size()},
            {QStringLiteral("ranked"), serializedRanked},
            {QStringLiteral("shortlist"), serializedShortlist},
            {QStringLiteral("rejected"), rejected},
        };
    }

} // namespace ssa::theme_lab
