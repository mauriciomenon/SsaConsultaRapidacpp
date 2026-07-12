#include "tools/theme_lab/ThemePaletteLab.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <array>
#include <stdexcept>
#include <utility>

namespace {

    QJsonObject ayuMirageScheme() {
        static constexpr std::array<const char*, 16> keys{
            "base00", "base01", "base02", "base03", "base04", "base05", "base06", "base07",
            "base08", "base09", "base0A", "base0B", "base0C", "base0D", "base0E", "base0F",
        };
        static constexpr std::array<const char*, 16> colors{
            "#1f2430", "#242936", "#323844", "#4a5059", "#707a8c", "#cccac2", "#d9d7ce", "#f3f4f5",
            "#f28779", "#ffad66", "#ffd173", "#d5ff80", "#95e6cb", "#73d0ff", "#d4bfff", "#f27983",
        };

        QJsonObject baseColors;
        for (std::size_t index = 0; index < keys.size(); ++index) {
            baseColors.insert(QString::fromLatin1(keys[index]), QString::fromLatin1(colors[index]));
        }
        return {
            {QStringLiteral("slug"), QStringLiteral("ayu-mirage")},
            {QStringLiteral("variant"), QStringLiteral("dark")},
            {QStringLiteral("palette"), baseColors},
        };
    }

    ssa::theme_lab::Base16Scheme parseRequiredScheme(const QJsonObject& object) {
        QStringList errors;
        auto scheme = ssa::theme_lab::parseBase16Scheme(object, errors);
        if (!scheme.has_value()) {
            throw std::runtime_error(errors.join(QStringLiteral(", ")).toStdString());
        }
        return std::move(scheme).value();
    }

} // namespace

TEST_CASE("theme lab parses a complete base16 scheme") {
    const auto scheme = parseRequiredScheme(ayuMirageScheme());

    CHECK(scheme.slug == QStringLiteral("ayu-mirage"));
    CHECK(scheme.dark);
    CHECK(scheme.colors.front() == QColor(QStringLiteral("#1f2430")));
    CHECK(scheme.colors.back() == QColor(QStringLiteral("#f27983")));
}

TEST_CASE("theme lab rejects incomplete and malformed base16 schemes") {
    QJsonObject malformed = ayuMirageScheme();
    QJsonObject colors = malformed.value(QStringLiteral("palette")).toObject();
    colors.remove(QStringLiteral("base0F"));
    colors.insert(QStringLiteral("base0A"), QStringLiteral("not-a-color"));
    malformed.insert(QStringLiteral("palette"), colors);

    QStringList errors;
    const auto scheme = ssa::theme_lab::parseBase16Scheme(malformed, errors);

    CHECK_FALSE(scheme.has_value());
    CHECK(errors.size() == 2);
}

TEST_CASE("theme lab maps base16 to the complete semantic palette") {
    const auto scheme = parseRequiredScheme(ayuMirageScheme());
    const auto palette = ssa::theme_lab::buildSemanticPalette(scheme);

    CHECK(palette.roles.size() == 20);
    CHECK(palette.roles.contains(QStringLiteral("window")));
    CHECK(palette.roles.contains(QStringLiteral("rowSelected")));
    CHECK(palette.roles.value(QStringLiteral("window")) !=
          palette.roles.value(QStringLiteral("surface")));
    CHECK(palette.roles.value(QStringLiteral("panel")) !=
          palette.roles.value(QStringLiteral("panelRaised")));
    CHECK(ssa::theme_lab::validateSemanticPalette(palette).isEmpty());
}

TEST_CASE("theme lab always chooses accessible text for a midtone accent") {
    QJsonObject object = ayuMirageScheme();
    QJsonObject colors = object.value(QStringLiteral("palette")).toObject();
    colors.insert(QStringLiteral("base0D"), QStringLiteral("#777777"));
    object.insert(QStringLiteral("palette"), colors);
    const auto scheme = parseRequiredScheme(object);
    const auto palette = ssa::theme_lab::buildSemanticPalette(scheme);

    CHECK(ssa::theme_lab::validateSemanticPalette(palette).isEmpty());
}

TEST_CASE("theme lab rejects inaccessible and glaring semantic palettes") {
    ssa::theme_lab::SemanticPalette invalid;
    invalid.slug = QStringLiteral("invalid-light");
    invalid.dark = false;
    static const std::array<const char*, 20> roles{
        "window",      "surface",    "panel",        "panelRaised",  "header",
        "tableHeader", "border",     "borderSoft",   "text",         "mutedText",
        "accent",      "accentText", "accentSoft",   "accentStrong", "link",
        "danger",      "dangerSoft", "dangerStrong", "rowAlt",       "rowSelected",
    };
    for (const char* role : roles) {
        invalid.roles.insert(QString::fromLatin1(role), QColor(QStringLiteral("#ffffff")));
    }

    const QStringList errors = ssa::theme_lab::validateSemanticPalette(invalid);

    CHECK_FALSE(errors.isEmpty());
}

TEST_CASE("theme lab rejects interactive backgrounds without an AA foreground") {
    const auto scheme = parseRequiredScheme(ayuMirageScheme());
    auto palette = ssa::theme_lab::buildSemanticPalette(scheme);
    palette.roles.insert(QStringLiteral("accentSoft"), QColor(QStringLiteral("#587484")));

    const QStringList validationErrors = ssa::theme_lab::validateSemanticPalette(palette);

    CHECK(validationErrors.contains(QStringLiteral("interactive contrast failed: accentSoft")));
}

TEST_CASE("theme lab keeps the best palette when perceptual duplicates collide") {
    const auto scheme = parseRequiredScheme(ayuMirageScheme());
    auto stronger = ssa::theme_lab::buildSemanticPalette(scheme);
    stronger.slug = QStringLiteral("stronger");
    stronger.score = 90.0;
    auto weaker = stronger;
    weaker.slug = QStringLiteral("weaker");
    weaker.score = 40.0;

    const auto ranked = ssa::theme_lab::rankAndDeduplicate({weaker, stronger}, 10);

    REQUIRE(ranked.size() == 1);
    CHECK(ranked.front().slug == QStringLiteral("stronger"));
    CHECK(ssa::theme_lab::paletteDistance(stronger, weaker) == 0.0);
}

TEST_CASE("theme lab serializes deterministic QML palette roles") {
    const auto scheme = parseRequiredScheme(ayuMirageScheme());
    const auto palette = ssa::theme_lab::buildSemanticPalette(scheme);

    const QJsonObject serialized = ssa::theme_lab::serializePalette(palette);

    CHECK(serialized.value(QStringLiteral("slug")).toString() == QStringLiteral("ayu-mirage"));
    CHECK(serialized.value(QStringLiteral("isDark")).toBool());
    CHECK(serialized.value(QStringLiteral("roles")).toObject().size() == 20);
}

TEST_CASE("theme lab keeps semantic gates after RGB hex serialization") {
    QJsonObject object = ayuMirageScheme();
    QJsonObject colors = object.value(QStringLiteral("palette")).toObject();
    colors.insert(QStringLiteral("base0D"), QStringLiteral("#399ee6"));
    object.insert(QStringLiteral("palette"), colors);
    const auto scheme = parseRequiredScheme(object);
    const auto palette = ssa::theme_lab::buildSemanticPalette(scheme);
    const QJsonObject serialized = ssa::theme_lab::serializePalette(palette);

    ssa::theme_lab::SemanticPalette roundtrip;
    roundtrip.slug = palette.slug;
    roundtrip.dark = palette.dark;
    const QJsonObject serializedRoles = serialized.value(QStringLiteral("roles")).toObject();
    for (auto iterator = serializedRoles.begin(); iterator != serializedRoles.end(); ++iterator) {
        roundtrip.roles.insert(iterator.key(), QColor(iterator.value().toString()));
    }

    const QStringList roundtripErrors = ssa::theme_lab::validateSemanticPalette(roundtrip);
    INFO(roundtripErrors.join(QStringLiteral(", ")).toStdString());
    CHECK(roundtripErrors.isEmpty());
}

TEST_CASE("theme lab preserves Flexoki text contrast after RGB hex serialization") {
    const QJsonObject object{
        {QStringLiteral("slug"), QStringLiteral("flexoki-dark")},
        {QStringLiteral("variant"), QStringLiteral("dark")},
        {QStringLiteral("palette"),
         QJsonObject{
             {QStringLiteral("base00"), QStringLiteral("#100f0f")},
             {QStringLiteral("base01"), QStringLiteral("#1c1b1a")},
             {QStringLiteral("base02"), QStringLiteral("#282726")},
             {QStringLiteral("base03"), QStringLiteral("#575653")},
             {QStringLiteral("base04"), QStringLiteral("#878580")},
             {QStringLiteral("base05"), QStringLiteral("#cecdc3")},
             {QStringLiteral("base06"), QStringLiteral("#e6e4d9")},
             {QStringLiteral("base07"), QStringLiteral("#fffcf0")},
             {QStringLiteral("base08"), QStringLiteral("#d14d41")},
             {QStringLiteral("base09"), QStringLiteral("#da702c")},
             {QStringLiteral("base0A"), QStringLiteral("#d0a215")},
             {QStringLiteral("base0B"), QStringLiteral("#879a39")},
             {QStringLiteral("base0C"), QStringLiteral("#3aa99f")},
             {QStringLiteral("base0D"), QStringLiteral("#4385be")},
             {QStringLiteral("base0E"), QStringLiteral("#8b7ec8")},
             {QStringLiteral("base0F"), QStringLiteral("#ce5d97")},
         }},
    };
    const auto scheme = parseRequiredScheme(object);
    const QJsonObject serialized =
        ssa::theme_lab::serializePalette(ssa::theme_lab::buildSemanticPalette(scheme));

    ssa::theme_lab::SemanticPalette roundtrip;
    roundtrip.slug = scheme.slug;
    roundtrip.dark = scheme.dark;
    const QJsonObject serializedRoles = serialized.value(QStringLiteral("roles")).toObject();
    for (auto iterator = serializedRoles.begin(); iterator != serializedRoles.end(); ++iterator) {
        roundtrip.roles.insert(iterator.key(), QColor(iterator.value().toString()));
    }
    const QStringList roundtripErrors = ssa::theme_lab::validateSemanticPalette(roundtrip);
    INFO(roundtripErrors.join(QStringLiteral(", ")).toStdString());
    CHECK(roundtripErrors.isEmpty());
}

TEST_CASE("theme lab keeps Kanagawa surfaces above the dark luminance floor") {
    const QJsonObject object{
        {QStringLiteral("slug"), QStringLiteral("kanagawa")},
        {QStringLiteral("variant"), QStringLiteral("dark")},
        {QStringLiteral("palette"),
         QJsonObject{
             {QStringLiteral("base00"), QStringLiteral("#1f1f28")},
             {QStringLiteral("base01"), QStringLiteral("#16161d")},
             {QStringLiteral("base02"), QStringLiteral("#223249")},
             {QStringLiteral("base03"), QStringLiteral("#54546d")},
             {QStringLiteral("base04"), QStringLiteral("#727169")},
             {QStringLiteral("base05"), QStringLiteral("#dcd7ba")},
             {QStringLiteral("base06"), QStringLiteral("#c8c093")},
             {QStringLiteral("base07"), QStringLiteral("#717c7c")},
             {QStringLiteral("base08"), QStringLiteral("#c34043")},
             {QStringLiteral("base09"), QStringLiteral("#ffa066")},
             {QStringLiteral("base0A"), QStringLiteral("#c0a36e")},
             {QStringLiteral("base0B"), QStringLiteral("#76946a")},
             {QStringLiteral("base0C"), QStringLiteral("#6a9589")},
             {QStringLiteral("base0D"), QStringLiteral("#7e9cd8")},
             {QStringLiteral("base0E"), QStringLiteral("#957fb8")},
             {QStringLiteral("base0F"), QStringLiteral("#d27e99")},
         }},
    };
    const auto scheme = parseRequiredScheme(object);

    const QStringList validationErrors =
        ssa::theme_lab::validateSemanticPalette(ssa::theme_lab::buildSemanticPalette(scheme));

    INFO(validationErrors.join(QStringLiteral(", ")).toStdString());
    CHECK(validationErrors.isEmpty());
}

TEST_CASE("theme lab reads builder output and reports malformed files") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    QFile validFile(directory.filePath(QStringLiteral("ayu-mirage.json")));
    REQUIRE(validFile.open(QIODevice::WriteOnly));
    REQUIRE(validFile.write(QJsonDocument(ayuMirageScheme()).toJson()) > 0);
    validFile.close();

    QFile invalidFile(directory.filePath(QStringLiteral("invalid.json")));
    REQUIRE(invalidFile.open(QIODevice::WriteOnly));
    REQUIRE(invalidFile.write("{invalid-json") > 0);
    invalidFile.close();

    QStringList errors;
    const auto catalog = ssa::theme_lab::readCatalog(directory.path(), errors);

    REQUIRE(catalog.size() == 1);
    CHECK(catalog.front().slug == QStringLiteral("ayu-mirage"));
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().contains(QStringLiteral("invalid.json")));
}

TEST_CASE("theme lab analyzes a complete catalog") {
    const auto scheme = parseRequiredScheme(ayuMirageScheme());

    const QJsonObject report = ssa::theme_lab::analyzeCatalog({scheme}, 24);

    CHECK(report.value(QStringLiteral("inputCount")).toInt() == 1);
    CHECK(report.value(QStringLiteral("acceptedCount")).toInt() == 1);
    CHECK(report.value(QStringLiteral("rejectedCount")).toInt() == 0);
    const QJsonArray ranked = report.value(QStringLiteral("ranked")).toArray();
    REQUIRE(ranked.size() == 1);
    CHECK(ranked.at(0).toObject().value(QStringLiteral("slug")).toString() ==
          QStringLiteral("ayu-mirage"));
    const QJsonArray shortlist = report.value(QStringLiteral("shortlist")).toArray();
    REQUIRE(shortlist.size() == 1);
    CHECK(shortlist.at(0).toObject().value(QStringLiteral("slug")).toString() ==
          QStringLiteral("ayu-mirage"));
}
