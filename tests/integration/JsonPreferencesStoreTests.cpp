#include "domain/ColumnCatalog.h"
#include "infra/preferences/JsonFilterPresetStore.h"
#include "infra/preferences/JsonPersistenceSupport.h"
#include "infra/preferences/JsonUserPreferencesStore.h"
#include "qt/FilesystemPath.h"

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>

#include <filesystem>
#include <string>
#include <utility>

namespace {

    constexpr qsizetype kOneMebibyte = qsizetype{1024} * 1024;

    class ShortWriteDevice final : public QIODevice {
      public:
        explicit ShortWriteDevice(const qint64 firstWriteBytes)
            : firstWriteBytes_(firstWriteBytes) {}

      protected:
        qint64 readData(char*, qint64) override {
            return -1;
        }

        qint64 writeData(const char*, const qint64 maximumSize) override {
            if (!wroteOnce_) {
                wroteOnce_ = true;
                return std::min(firstWriteBytes_, maximumSize);
            }
            setErrorString("simulated short write failure");
            return -1;
        }

      private:
        qint64 firstWriteBytes_ = 0;
        bool wroteOnce_ = false;
    };

    void writeBytes(const std::filesystem::path& path, const QByteArray& payload) {
        QFile file(QString::fromStdString(path.string()));
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        REQUIRE(file.write(payload) == payload.size());
        file.close();
    }

    void writeObject(const std::filesystem::path& path, const QJsonObject& object) {
        writeBytes(path, QJsonDocument(object).toJson(QJsonDocument::Compact));
    }

    QByteArray readBytes(const std::filesystem::path& path) {
        QFile file(QString::fromStdString(path.string()));
        REQUIRE(file.open(QIODevice::ReadOnly));
        return file.readAll();
    }

} // namespace

TEST_CASE("json preferences store saves user preference snapshot") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);

    ssa::ports::UserPreferencesSnapshot snapshot;
    snapshot.pageSize = 50;
    snapshot.theme = "tokyo-night";
    snapshot.density = "compact";
    snapshot.detailsVisible = false;
    snapshot.detailsPanelWidth = 520;
    snapshot.sortColumnKey = "situacao";
    snapshot.sortAscending = true;
    snapshot.visibleColumns = {"numero_ssa", "situacao"};
    snapshot.columnWidths = {{"numero_ssa", 140}};
    snapshot.savedFilters = {ssa::ports::SavedFilterSnapshot{
        .name = "braba",
        .filters =
            ssa::ports::FilterPreferencesSnapshot{.searchText = "svp", .quickSector = "MEG2"}}};
    snapshot.filters.searchText = "isolamento";
    snapshot.filters.quickSector = "IEE3";
    snapshot.filters.excludeScaSesSte = false;
    snapshot.filters.columnFilters = {{"situacao", "=ADM"}};
    snapshot.filters.advancedTextFilters = {{"setor_executor", "=MEG2"}, {"situacao", "=APV"}};
    snapshot.filters.advancedWeekColumnKey = "semana_executada";
    snapshot.filters.advancedYear = "2025";
    snapshot.filters.advancedWeek = "3";
    snapshot.filters.issueYear = "2026";
    snapshot.filters.executionYear = "2027";
    snapshot.filters.reprogrammingMode = "lte";
    snapshot.filters.reprogrammingValues = "1,3,5";
    snapshot.filters.issueWeekStart = "202601";
    snapshot.filters.issueWeekEnd = "202620";
    snapshot.filters.executionWeekStart = "202701";
    snapshot.filters.executionWeekEnd = "202720";
    snapshot.filters.derivationMode = "derived";
    snapshot.filters.onlyReprogrammed = false;

    store.save(snapshot);
    const auto loaded = store.load();

    REQUIRE(loaded.pageSize == 50);
    REQUIRE(loaded.theme == "tokyo-night");
    REQUIRE(loaded.density == "compact");
    REQUIRE_FALSE(loaded.detailsVisible);
    REQUIRE(loaded.detailsPanelWidth == 520);
    REQUIRE(loaded.sortColumnKey == "situacao");
    REQUIRE(loaded.sortAscending);
    REQUIRE(loaded.visibleColumns == std::vector<std::string>{"numero_ssa", "situacao"});
    REQUIRE(loaded.columnWidths.at("numero_ssa") == 140);
    REQUIRE(loaded.savedFilters.size() == 1);
    REQUIRE(loaded.savedFilters.front().name == "braba");
    REQUIRE(loaded.savedFilters.front().filters.searchText == "svp");
    REQUIRE(loaded.savedFilters.front().filters.quickSector == "MEG2");
    REQUIRE(loaded.filters.searchText == "isolamento");
    REQUIRE(loaded.filters.quickSector == "IEE3");
    REQUIRE_FALSE(loaded.filters.excludeScaSesSte);
    REQUIRE(loaded.filters.columnFilters.at("situacao") == "=ADM");
    REQUIRE(loaded.filters.advancedTextFilters.at("setor_executor") == "=MEG2");
    REQUIRE(loaded.filters.advancedTextFilters.at("situacao") == "=APV");
    REQUIRE(loaded.filters.advancedWeekColumnKey == "semana_executada");
    REQUIRE(loaded.filters.advancedYear == "2025");
    REQUIRE(loaded.filters.advancedWeek == "3");
    REQUIRE(loaded.filters.issueYear == "2026");
    REQUIRE(loaded.filters.executionYear == "2027");
    REQUIRE(loaded.filters.reprogrammingMode == "lte");
    REQUIRE(loaded.filters.reprogrammingValues == "1,3,5");
    REQUIRE(loaded.filters.issueWeekStart == "202601");
    REQUIRE(loaded.filters.issueWeekEnd == "202620");
    REQUIRE(loaded.filters.executionWeekStart == "202701");
    REQUIRE(loaded.filters.executionWeekEnd == "202720");
    REQUIRE(loaded.filters.derivationMode == "derived");
    REQUIRE_FALSE(loaded.filters.onlyReprogrammed);
}

TEST_CASE("json preferences preserve unicode file paths") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = ssa::qt::toFileSystemPath(
        directory.filePath(QString::fromUtf8("preferencias-acao-\xE6\xBC\xA2.json")));
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    ssa::ports::UserPreferencesSnapshot snapshot;
    snapshot.theme = "gruvbox";

    store.save(snapshot);

    REQUIRE(std::filesystem::is_regular_file(path));
    REQUIRE(store.load().theme == "gruvbox");
}

TEST_CASE("json preferences canonicalize legacy reprogramming filters") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);

    SECTION("visible values take priority over legacy equals and only") {
        writeObject(path, QJsonObject{{"schema_version", 12},
                                      {"reprogramming_values", "1,3"},
                                      {"reprogramming_equals", "2"},
                                      {"only_reprogrammed", true}});

        const auto loaded = store.load();

        REQUIRE(loaded.filters.reprogrammingValues == "1,3");
        REQUIRE_FALSE(loaded.filters.onlyReprogrammed);
    }

    SECTION("legacy equals becomes the visible value filter") {
        writeObject(path, QJsonObject{{"schema_version", 12},
                                      {"reprogramming_equals", "2"},
                                      {"only_reprogrammed", true}});

        const auto loaded = store.load();

        REQUIRE(loaded.filters.reprogrammingValues == "2");
        REQUIRE_FALSE(loaded.filters.onlyReprogrammed);
    }

    SECTION("explicit empty visible values suppress legacy equals") {
        writeObject(path, QJsonObject{{"schema_version", 12},
                                      {"reprogramming_values", ""},
                                      {"reprogramming_equals", "2"},
                                      {"only_reprogrammed", true}});

        const auto loaded = store.load();

        REQUIRE(loaded.filters.reprogrammingValues.empty());
        REQUIRE(loaded.filters.onlyReprogrammed);
    }

    SECTION("only remains active without a value filter") {
        writeObject(path, QJsonObject{{"schema_version", 12}, {"only_reprogrammed", true}});

        REQUIRE(store.load().filters.onlyReprogrammed);
    }
}

TEST_CASE("json preferences stop writing legacy reprogramming equals") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    ssa::ports::UserPreferencesSnapshot snapshot;
    snapshot.filters.reprogrammingValues = "2";
    snapshot.filters.onlyReprogrammed = true;

    store.save(snapshot);
    const auto root = QJsonDocument::fromJson(readBytes(path)).object();

    REQUIRE_FALSE(root.contains("reprogramming_equals"));
    REQUIRE_FALSE(root.value("only_reprogrammed").toBool());
}

TEST_CASE("json preferences reject duplicate visible columns") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    writeObject(
        path, QJsonObject{{"schema_version", 12},
                          {"visible_columns", QJsonArray{"numero_ssa", "situacao", "numero_ssa"}}});

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);

    std::string error;
    try {
        (void)store.load();
    } catch (const std::runtime_error& exc) {
        error = exc.what();
    }
    REQUIRE(error == "duplicate visible column: numero_ssa");
}

TEST_CASE("user preference snapshots and saved documents use schema 12") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    ssa::ports::UserPreferencesSnapshot snapshot;

    REQUIRE(snapshot.schemaVersion == 12);
    REQUIRE(snapshot.schemaVersion == ssa::ports::kCurrentUserPreferencesSchemaVersion);
    snapshot.schemaVersion = 3;
    store.save(snapshot);

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(readBytes(path), &parseError);
    REQUIRE(parseError.error == QJsonParseError::NoError);
    REQUIRE(document.object().value("schema_version").toInt() ==
            ssa::ports::kCurrentUserPreferencesSchemaVersion);
    REQUIRE(store.load().schemaVersion == ssa::ports::kCurrentUserPreferencesSchemaVersion);
}

TEST_CASE("json preferences store rejects missing invalid and future schemas") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);

    SECTION("missing") {
        writeObject(path, QJsonObject{{"theme", "ssa-dark"}});
        REQUIRE_THROWS(store.load());
    }
    SECTION("invalid type") {
        writeObject(path, QJsonObject{{"schema_version", "12"}});
        REQUIRE_THROWS(store.load());
    }
    SECTION("invalid value") {
        writeObject(path, QJsonObject{{"schema_version", 0}});
        REQUIRE_THROWS(store.load());
    }
    SECTION("future") {
        writeObject(path, QJsonObject{{"schema_version", 13}});
        REQUIRE_THROWS(store.load());
    }
}

TEST_CASE("json filter preset store rejects missing invalid and future schemas") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "preset.json";
    const ssa::infra::preferences::JsonFilterPresetStore store;

    SECTION("missing") {
        writeObject(path, QJsonObject{{"quick_sector", "IEE3"}});
        REQUIRE_THROWS(store.load(path));
    }
    SECTION("invalid type") {
        writeObject(path, QJsonObject{{"schema_version", "1"}});
        REQUIRE_THROWS(store.load(path));
    }
    SECTION("invalid value") {
        writeObject(path, QJsonObject{{"schema_version", 0}});
        REQUIRE_THROWS(store.load(path));
    }
    SECTION("future") {
        writeObject(path, QJsonObject{{"schema_version", 2}});
        REQUIRE_THROWS(store.load(path));
    }
}

TEST_CASE("json preference stores reject files larger than one mebibyte") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    SECTION("user preferences") {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
        writeObject(path,
                    QJsonObject{{"schema_version", 12}, {"padding", QString(kOneMebibyte, 'x')}});
        const ssa::infra::preferences::JsonUserPreferencesStore store(path);
        REQUIRE_THROWS(store.load());
    }
    SECTION("filter preset") {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "preset.json";
        writeObject(path,
                    QJsonObject{{"schema_version", 1}, {"padding", QString(kOneMebibyte, 'x')}});
        const ssa::infra::preferences::JsonFilterPresetStore store;
        REQUIRE_THROWS(store.load(path));
    }
}

TEST_CASE("oversized json replacement preserves the previous file") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    SECTION("user preferences") {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
        const QByteArray original{R"JSON({"schema_version":12,"theme":"gruvbox"})JSON"};
        writeBytes(path, original);
        const ssa::infra::preferences::JsonUserPreferencesStore store(path);
        ssa::ports::UserPreferencesSnapshot snapshot;
        const std::string boundaryExpression(ssa::ports::kMaxFilterExpressionLength, 'x');
        for (int index = 0; index < 200; ++index) {
            ssa::ports::SavedFilterSnapshot saved;
            saved.name = "filter-" + std::to_string(index);
            saved.filters.searchText = boundaryExpression;
            saved.filters.quickSector = boundaryExpression;
            snapshot.savedFilters.push_back(std::move(saved));
        }

        try {
            store.save(snapshot);
            FAIL("oversized preferences payload must be rejected");
        } catch (const std::runtime_error& error) {
            REQUIRE(std::string{error.what()}.find("size limit") != std::string::npos);
        }
        REQUIRE(readBytes(path) == original);
    }
    SECTION("filter preset") {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "preset.json";
        const QByteArray original{R"JSON({"schema_version":1,"quick_sector":"IEE3"})JSON"};
        writeBytes(path, original);
        const ssa::infra::preferences::JsonFilterPresetStore store;
        ssa::ports::FilterPresetSnapshot snapshot;
        snapshot.filters.searchText.assign(static_cast<std::size_t>(kOneMebibyte), 'x');

        REQUIRE_THROWS(store.save(path, snapshot));
        REQUIRE(readBytes(path) == original);
    }
}

TEST_CASE("json persistence rejects a short write before commit") {
    ShortWriteDevice output(3);
    REQUIRE(output.open(QIODevice::WriteOnly));

    REQUIRE_THROWS(ssa::infra::preferences::json_persistence::writeFully(
        output, QByteArray{"payload"}, "preferences file", std::filesystem::path{"prefs.json"}));
}

TEST_CASE("json persistence reports commit failure and preserves the previous file") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto root = std::filesystem::path{directory.path().toStdString()};
    const auto targetDirectory = root / "target";
    std::filesystem::create_directories(targetDirectory);
    const auto path = targetDirectory / "prefs.json";
    const QByteArray original{R"JSON({"schema_version":12,"theme":"gruvbox"})JSON"};
    writeBytes(path, original);

    QSaveFile output(QString::fromStdString(path.string()));
    output.setDirectWriteFallback(false);
    REQUIRE(output.open(QIODevice::WriteOnly));
    ssa::infra::preferences::json_persistence::writeFully(
        output, QByteArray{R"JSON({"schema_version":12,"theme":"grayscalepy"})JSON"},
        "preferences file", path);
    output.cancelWriting();

    REQUIRE_THROWS(
        ssa::infra::preferences::json_persistence::commitOrThrow(output, "preferences file", path));
    REQUIRE(readBytes(path) == original);
}

TEST_CASE("json persistence accepts exactly one mebibyte") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "boundary.json";
    const QByteArray payload(kOneMebibyte, 'x');
    writeBytes(path, payload);

    REQUIRE(ssa::infra::preferences::json_persistence::readBounded(path, "boundary file") ==
            payload);
}

TEST_CASE("json preferences store roundtrips every supported theme through two save cycles") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    std::size_t pythonThemeCount = 0;

    for (const auto theme : ssa::ports::kThemeValues) {
        if (!theme.ends_with("py")) {
            continue;
        }
        ++pythonThemeCount;
        ssa::ports::UserPreferencesSnapshot first;
        first.theme = theme;
        first.pageSize = 25;
        store.save(first);

        auto second = store.load();
        REQUIRE(second.schemaVersion == ssa::ports::kCurrentUserPreferencesSchemaVersion);
        REQUIRE(second.theme == theme);
        REQUIRE(second.pageSize == 25);

        second.pageSize = 50;
        store.save(second);
        const auto third = store.load();
        REQUIRE(third.schemaVersion == ssa::ports::kCurrentUserPreferencesSchemaVersion);
        REQUIRE(third.theme == theme);
        REQUIRE(third.pageSize == 50);
    }

    REQUIRE(pythonThemeCount == 13);
}

TEST_CASE("json preference limits accept their exact boundaries") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);

    SECTION("two hundred saved filters") {
        ssa::ports::UserPreferencesSnapshot snapshot;
        for (std::size_t index = 0; index < ssa::ports::kMaxSavedFilterCount; ++index) {
            snapshot.savedFilters.push_back({"filter-" + std::to_string(index), {}});
        }
        REQUIRE_NOTHROW(store.save(snapshot));
        REQUIRE(store.load().savedFilters.size() == ssa::ports::kMaxSavedFilterCount);
    }
    SECTION("saved filter name with 128 characters") {
        ssa::ports::UserPreferencesSnapshot snapshot;
        snapshot.savedFilters.push_back(
            {std::string(ssa::ports::kMaxSavedFilterNameLength, 'n'), {}});
        REQUIRE_NOTHROW(store.save(snapshot));
        REQUIRE(store.load().savedFilters.front().name.size() ==
                ssa::ports::kMaxSavedFilterNameLength);
    }
    SECTION("filter expression with 4096 characters") {
        ssa::ports::UserPreferencesSnapshot snapshot;
        snapshot.filters.advancedTextFilters = {
            {"situacao", std::string(ssa::ports::kMaxFilterExpressionLength, 'x')}};
        REQUIRE_NOTHROW(store.save(snapshot));
        REQUIRE(store.load().filters.advancedTextFilters.at("situacao").size() ==
                ssa::ports::kMaxFilterExpressionLength);
    }
}

TEST_CASE("json preferences store enforces saved filter count and name limits") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);

    SECTION("more than 200 filters on save") {
        ssa::ports::UserPreferencesSnapshot snapshot;
        for (int index = 0; index < 201; ++index) {
            snapshot.savedFilters.push_back({"filter-" + std::to_string(index), {}});
        }
        REQUIRE_THROWS(store.save(snapshot));
    }
    SECTION("more than 200 filters on load") {
        QJsonArray filters;
        for (int index = 0; index < 201; ++index) {
            filters.append(
                QJsonObject{{"name", QString("filter-%1").arg(index)}, {"filters", QJsonObject{}}});
        }
        writeObject(path, QJsonObject{{"schema_version", 12}, {"saved_filters", filters}});
        REQUIRE_THROWS(store.load());
    }
    SECTION("name longer than 128 characters on save") {
        ssa::ports::UserPreferencesSnapshot snapshot;
        snapshot.savedFilters.push_back({std::string(129, 'n'), {}});
        REQUIRE_THROWS(store.save(snapshot));
    }
    SECTION("name longer than 128 characters on load") {
        const QJsonArray filters{
            QJsonObject{{"name", QString(129, 'n')}, {"filters", QJsonObject{}}}};
        writeObject(path, QJsonObject{{"schema_version", 12}, {"saved_filters", filters}});
        REQUIRE_THROWS(store.load());
    }
}

TEST_CASE("json preference codecs reject filter expressions longer than 4096 characters") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const std::string oversizedExpression(4097, 'x');

    SECTION("user preferences save") {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
        const ssa::infra::preferences::JsonUserPreferencesStore store(path);
        ssa::ports::UserPreferencesSnapshot snapshot;
        snapshot.filters.advancedTextFilters = {{"situacao", oversizedExpression}};
        REQUIRE_THROWS(store.save(snapshot));
    }
    SECTION("user preferences load") {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
        const ssa::infra::preferences::JsonUserPreferencesStore store(path);
        writeObject(path, QJsonObject{{"schema_version", 12},
                                      {"advanced_text_filters",
                                       QJsonObject{{"situacao", QString(4097, 'x')}}}});
        REQUIRE_THROWS(store.load());
    }
    SECTION("filter preset save") {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "preset.json";
        const ssa::infra::preferences::JsonFilterPresetStore store;
        ssa::ports::FilterPresetSnapshot snapshot;
        snapshot.filters.columnFilters = {{"situacao", oversizedExpression}};
        REQUIRE_THROWS(store.save(path, snapshot));
    }
    SECTION("filter preset load") {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "preset.json";
        const ssa::infra::preferences::JsonFilterPresetStore store;
        writeObject(path,
                    QJsonObject{{"schema_version", 1},
                                {"column_filters", QJsonObject{{"situacao", QString(4097, 'x')}}}});
        REQUIRE_THROWS(store.load(path));
    }
}

TEST_CASE("json preference codecs count expression limits as unicode characters") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const QString expression(4096, QChar{0x00E9});
    ssa::ports::UserPreferencesSnapshot snapshot;
    snapshot.filters.advancedTextFilters = {{"situacao", expression.toStdString()}};

    REQUIRE_NOTHROW(store.save(snapshot));
    REQUIRE(store.load().filters.advancedTextFilters.at("situacao") == expression.toStdString());
}

TEST_CASE("json preferences store uses ssa dark theme for a clean profile") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.theme == "ssa-dark");
    REQUIRE(loaded.visibleColumns == ssa::domain::ColumnCatalog::defaultVisibleKeys());
}

TEST_CASE("json preferences store preserves existing valid manual theme") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"JSON({"schema_version":4,"theme":"gruvbox"})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(loaded.theme == "gruvbox");
}

TEST_CASE("json preferences store keeps default columns when saved list is invalid") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"JSON({"schema_version":12,"visible_columns":[1,false,null]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.visibleColumns == ssa::domain::ColumnCatalog::defaultVisibleKeys());
}

TEST_CASE("json preferences store prunes legacy hidden visible columns on load") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":12,"visible_columns":["numero_ssa","equipamento","grau_prioridade_emissao","grau_prioridade_planejamento","situacao"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.visibleColumns == std::vector<std::string>{"numero_ssa", "situacao"});
}

TEST_CASE("json preferences store prunes legacy description location visibility on load") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":12,"visible_columns":["numero_ssa","descricao_localizacao","situacao"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.visibleColumns == std::vector<std::string>{"numero_ssa", "situacao"});
}

TEST_CASE("json preferences store migrates legacy derived count visibility") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":1,"visible_columns":["numero_ssa","setor_executor","situacao"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(loaded.filters.quickSector == "IEE3");
    REQUIRE(loaded.visibleColumns ==
            std::vector<std::string>{"numero_ssa", "setor_executor", "qtd_derivadas", "situacao"});
}

TEST_CASE("json preferences store restores default quick sector when it is alone") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":3,"quick_sector":"","visible_columns":["numero_ssa","setor_executor","qtd_derivadas"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(loaded.filters.quickSector == "IEE3");
}

TEST_CASE("json preferences store keeps empty quick sector when other filters exist") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":3,"quick_sector":"","search_text":"manual","visible_columns":["numero_ssa","setor_executor","qtd_derivadas"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(loaded.filters.quickSector.empty());
    REQUIRE(loaded.filters.searchText == "manual");
}

TEST_CASE("json preferences store migrates only legacy default table widths") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":4,"column_widths":{"numero_ssa":110,"situacao":80,"localizacao_codigo":120,"setor_emissor":90,"setor_executor":91,"qtd_derivadas":72,"derivada_de":82,"data_cadastro":120,"semana_cadastro":95,"descricao_ssa":360,"solicitante":180,"responsavel_programacao":180,"responsavel_execucao":180,"semana_programada":95,"semana_executada":95}})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(loaded.columnWidths.at("numero_ssa") == 98);
    REQUIRE(loaded.columnWidths.at("situacao") == 60);
    REQUIRE(loaded.columnWidths.at("localizacao_codigo") == 84);
    REQUIRE(loaded.columnWidths.at("setor_emissor") == 68);
    REQUIRE(loaded.columnWidths.at("setor_executor") == 91);
    REQUIRE(loaded.columnWidths.at("qtd_derivadas") == 66);
    REQUIRE(loaded.columnWidths.at("derivada_de") == 88);
    REQUIRE(loaded.columnWidths.at("data_cadastro") == 100);
    REQUIRE(loaded.columnWidths.at("semana_cadastro") == 84);
    REQUIRE(loaded.columnWidths.at("descricao_ssa") == 640);
    REQUIRE(loaded.columnWidths.at("solicitante") == 240);
    REQUIRE(loaded.columnWidths.at("responsavel_programacao") == 250);
    REQUIRE(loaded.columnWidths.at("responsavel_execucao") == 250);
    REQUIRE(loaded.columnWidths.at("semana_programada") == 86);
    REQUIRE(loaded.columnWidths.at("semana_executada") == 86);
}

TEST_CASE("json preferences store migrates legacy default visible column order") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":8,"visible_columns":["numero_ssa","situacao","derivada_de","localizacao_codigo","semana_cadastro","data_cadastro","descricao_ssa","descricao_execucao","setor_emissor","setor_executor","qtd_derivadas","solicitante","responsavel_programacao","responsavel_execucao","semana_programada","semana_executada"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(loaded.visibleColumns == ssa::domain::ColumnCatalog::defaultVisibleKeys());
}

TEST_CASE("json preferences store migrates schema 10 default visible column order") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":10,"visible_columns":["numero_ssa","situacao","localizacao_codigo","setor_emissor","setor_executor","qtd_derivadas","descricao_ssa","data_cadastro","derivada_de","semana_cadastro","solicitante","responsavel_programacao","responsavel_execucao","semana_programada","semana_executada","descricao_execucao"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(loaded.visibleColumns == ssa::domain::ColumnCatalog::defaultVisibleKeys());
}

TEST_CASE("json preferences store removes cadastro and keeps requested detail column order") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":11,"visible_columns":["numero_ssa","situacao","localizacao_codigo","setor_emissor","setor_executor","qtd_derivadas","descricao_ssa","data_cadastro","derivada_de","semana_cadastro","solicitante","responsavel_programacao","responsavel_execucao"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(std::ranges::find(loaded.visibleColumns, "data_cadastro") ==
            loaded.visibleColumns.end());
    const auto description = std::ranges::find(loaded.visibleColumns, "descricao_ssa");
    REQUIRE(description != loaded.visibleColumns.end());
    REQUIRE(std::next(description) != loaded.visibleColumns.end());
    REQUIRE(*std::next(description) == "semana_cadastro");
    REQUIRE(std::next(description, 2) != loaded.visibleColumns.end());
    REQUIRE(*std::next(description, 2) == "solicitante");
    REQUIRE(std::next(description, 3) != loaded.visibleColumns.end());
    REQUIRE(*std::next(description, 3) == "derivada_de");
}

TEST_CASE("json preferences store compacts oversized derivation column width") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":7,"column_widths":{"derivada_de":220,"descricao_ssa":640}})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 12);
    REQUIRE(loaded.columnWidths.at("derivada_de") == 88);
    REQUIRE(loaded.columnWidths.at("descricao_ssa") == 640);
}

TEST_CASE("json preferences store drops invalid column filters") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":12,"column_filters":{"missing":"x","situacao":"APV"}})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE_FALSE(loaded.filters.columnFilters.contains("missing"));
    REQUIRE(loaded.filters.columnFilters.at("situacao") == "APV");
}

TEST_CASE("json preferences store drops invalid advanced text filters") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":12,"advanced_text_filters":{"missing":"x","setor_executor":"=MEG2"}})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE_FALSE(loaded.filters.advancedTextFilters.contains("missing"));
    REQUIRE(loaded.filters.advancedTextFilters.at("setor_executor") == "=MEG2");
}

TEST_CASE("json filter preset store saves only filter state") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "preset.json";
    const ssa::infra::preferences::JsonFilterPresetStore store;

    ssa::ports::FilterPresetSnapshot snapshot;
    snapshot.filters.quickSector = "MEG2";
    snapshot.filters.excludeScaSesSte = false;
    snapshot.filters.columnFilters = {{"situacao", "=APV"}};
    snapshot.filters.advancedTextFilters = {{"setor_executor", "=MMU3"}};
    snapshot.filters.issueYear = "2026";
    snapshot.filters.onlyReprogrammed = true;

    store.save(path, snapshot);
    const auto loaded = store.load(path);

    REQUIRE(loaded.filters.quickSector == "MEG2");
    REQUIRE_FALSE(loaded.filters.excludeScaSesSte);
    REQUIRE(loaded.filters.columnFilters.at("situacao") == "=APV");
    REQUIRE(loaded.filters.advancedTextFilters.at("setor_executor") == "=MMU3");
    REQUIRE(loaded.filters.issueYear == "2026");
    REQUIRE(loaded.filters.onlyReprogrammed);
}

TEST_CASE("json filter preset store canonicalizes legacy reprogramming equals") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "preset.json";
    writeObject(path, QJsonObject{{"schema_version", 1},
                                  {"reprogramming_equals", "4"},
                                  {"only_reprogrammed", true}});
    const ssa::infra::preferences::JsonFilterPresetStore store;

    const auto loaded = store.load(path);

    REQUIRE(loaded.filters.reprogrammingValues == "4");
    REQUIRE_FALSE(loaded.filters.onlyReprogrammed);
    store.save(path, loaded);
    REQUIRE_FALSE(
        QJsonDocument::fromJson(readBytes(path)).object().contains("reprogramming_equals"));
}
