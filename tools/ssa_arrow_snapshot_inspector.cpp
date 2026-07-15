#include <arrow/api.h>
#include <sqlite3.h>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

    constexpr std::int64_t kDefaultLimit = 1'000;
    constexpr std::int64_t kMaximumLimit = 100'000;

    struct Database final {
        sqlite3* handle = nullptr;

        ~Database() {
            if (handle != nullptr) {
                sqlite3_close(handle);
            }
        }
    };

    std::int64_t parseLimit(const char* text) {
        if (text == nullptr) {
            return kDefaultLimit;
        }
        std::int64_t value = 0;
        const std::string_view input{text};
        const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
        if (error != std::errc{} || end != input.data() + input.size() || value < 1 ||
            value > kMaximumLimit) {
            return 0;
        }
        return value;
    }

    arrow::Status appendText(arrow::StringBuilder& builder, sqlite3_stmt* statement,
                             const int column) {
        if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
            return builder.AppendNull();
        }
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));
        return builder.Append(text == nullptr ? "" : text);
    }

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: ssa_arrow_snapshot_inspector <database> [limit]\n";
        return 2;
    }
    const auto limit = parseLimit(argc == 3 ? argv[2] : nullptr);
    if (limit == 0) {
        std::cerr << "limit must be between 1 and 100000\n";
        return 2;
    }

    Database database;
    if (sqlite3_open_v2(argv[1], &database.handle, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        std::cerr << "cannot open database read-only\n";
        return 1;
    }
    constexpr const char* query = "SELECT numero_ssa, situacao, descricao_ssa, descricao_execucao "
                                  "FROM ssa_table ORDER BY numero_ssa LIMIT ?1";
    sqlite3_stmt* rawStatement = nullptr;
    if (sqlite3_prepare_v2(database.handle, query, -1, &rawStatement, nullptr) != SQLITE_OK) {
        std::cerr << "cannot prepare snapshot query\n";
        return 1;
    }
    const auto statementCleanup =
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{rawStatement, &sqlite3_finalize};
    if (sqlite3_bind_int64(rawStatement, 1, limit) != SQLITE_OK) {
        std::cerr << "cannot bind snapshot limit\n";
        return 1;
    }

    arrow::StringBuilder numberBuilder;
    arrow::StringBuilder statusBuilder;
    arrow::StringBuilder descriptionBuilder;
    arrow::StringBuilder executionBuilder;
    while (true) {
        const auto step = sqlite3_step(rawStatement);
        if (step == SQLITE_DONE) {
            break;
        }
        if (step != SQLITE_ROW) {
            std::cerr << "cannot read snapshot rows\n";
            return 1;
        }
        if (!appendText(numberBuilder, rawStatement, 0).ok() ||
            !appendText(statusBuilder, rawStatement, 1).ok() ||
            !appendText(descriptionBuilder, rawStatement, 2).ok() ||
            !appendText(executionBuilder, rawStatement, 3).ok()) {
            std::cerr << "cannot build Arrow snapshot columns\n";
            return 1;
        }
    }

    std::shared_ptr<arrow::Array> number;
    std::shared_ptr<arrow::Array> status;
    std::shared_ptr<arrow::Array> description;
    std::shared_ptr<arrow::Array> execution;
    if (!numberBuilder.Finish(&number).ok() || !statusBuilder.Finish(&status).ok() ||
        !descriptionBuilder.Finish(&description).ok() ||
        !executionBuilder.Finish(&execution).ok()) {
        std::cerr << "cannot finish Arrow snapshot columns\n";
        return 1;
    }
    const auto schema = arrow::schema({arrow::field("numero_ssa", arrow::utf8()),
                                       arrow::field("situacao", arrow::utf8()),
                                       arrow::field("descricao_ssa", arrow::utf8()),
                                       arrow::field("descricao_execucao", arrow::utf8())});
    const auto table = arrow::Table::Make(schema, {number, status, description, execution});
    if (!table->Validate().ok()) {
        std::cerr << "Arrow snapshot validation failed\n";
        return 1;
    }
    std::cout << "ARROW_SNAPSHOT rows=" << table->num_rows() << " columns=" << table->num_columns()
              << '\n';
    return 0;
}
