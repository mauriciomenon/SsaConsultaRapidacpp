#include "domain/SsaImportPolicy.h"
#include "domain/WhitespaceTrim.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <optional>
#include <ranges>
#include <string_view>

namespace ssa::domain {

    namespace {

        std::string valueFor(const SsaImportPolicy::Values& values, const std::string_view key) {
            const auto found = values.find(std::string{key});
            return found == values.end() ? std::string{} : trimWhitespace(found->second);
        }

        std::string uppercase(std::string value) {
            std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            return value;
        }

        bool isDateExemptStatus(const std::string& status) {
            static constexpr std::array<std::string_view, 3> exempt{"SCC", "ADI", "ASE"};
            const auto normalized = uppercase(trimWhitespace(status));
            return std::ranges::any_of(exempt, [&normalized](const std::string_view code) {
                return normalized == code || normalized.starts_with(std::string{code} + " ");
            });
        }

        bool isTerminalStatusValue(const std::string& status) {
            const auto normalized = uppercase(trimWhitespace(status));
            return normalized == "STE" || normalized.starts_with("STE ") || normalized == "SCA" ||
                   normalized.starts_with("SCA ");
        }

        bool isMetadataField(const std::string_view key) {
            return key == "arquivo_origem" || key == "data_planilha" ||
                   key == "data_criacao_arquivo" || key == "data_arquivo_origem";
        }

        bool isDateField(const std::string_view key) {
            static constexpr std::array<std::string_view, 8> dateFields{
                "data_planilha",       "data_criacao_arquivo",
                "data_arquivo_origem", "prazo_limite",
                "data_limite",         "data_inicio_programada",
                "data_programacao",    "data_inicio_reprogramada",
            };
            return std::ranges::find(dateFields, key) != dateFields.end() ||
                   key == "data_reprogramacao";
        }

        std::optional<int> parsePart(const std::string_view value);

        bool isValidWeek(const std::string& value) {
            const auto normalized = trimWhitespace(value);
            if (normalized.size() != 6 ||
                !std::ranges::all_of(
                    normalized, [](const unsigned char ch) { return std::isdigit(ch) != 0; })) {
                return false;
            }
            const auto year = parsePart(std::string_view{normalized}.substr(0, 4));
            const auto week = parsePart(std::string_view{normalized}.substr(4, 2));
            if (!year || *year < 1980 || *year > 2050 || !week || *week < 1 || *week > 53) {
                return false;
            }
            if (*week < 53) {
                return true;
            }
            const auto calendarYear = std::chrono::year{*year};
            const auto firstWeekday = std::chrono::weekday{
                std::chrono::sys_days{calendarYear / std::chrono::January / 1}};
            return firstWeekday.iso_encoding() == 4 ||
                   (calendarYear.is_leap() && firstWeekday.iso_encoding() == 3);
        }

        std::size_t completenessScore(const SsaImportPolicy::Values& values) {
            std::size_t score = 0;
            for (const auto& [key, value] : values) {
                if (!trimWhitespace(value).empty() && key != "numero_ssa" &&
                    !isMetadataField(key)) {
                    ++score;
                }
            }
            return score;
        }

        std::string lowercase(std::string value) {
            std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        std::optional<int> parsePart(const std::string_view value) {
            int parsed = 0;
            const auto [end, error] =
                std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (error != std::errc{} || end != value.data() + value.size()) {
                return std::nullopt;
            }
            return parsed;
        }

        struct SnapshotTimestamp {
            int year = 0;
            int month = 0;
            int day = 0;
            int hour = 0;
            int minute = 0;
            int second = 0;
        };

        bool validTimestamp(const SnapshotTimestamp& timestamp) {
            return timestamp.hour >= 0 && timestamp.hour < 24 && timestamp.minute >= 0 &&
                   timestamp.minute < 60 && timestamp.second >= 0 && timestamp.second < 60 &&
                   std::chrono::year_month_day{
                       std::chrono::year{timestamp.year},
                       std::chrono::month{static_cast<unsigned>(timestamp.month)},
                       std::chrono::day{static_cast<unsigned>(timestamp.day)}}
                       .ok();
        }

        bool applyMeridiem(SnapshotTimestamp& timestamp, std::string marker) {
            marker = uppercase(std::move(marker));
            if ((marker != "AM" && marker != "PM") || timestamp.hour < 1 || timestamp.hour > 12) {
                return false;
            }
            if (marker == "AM") {
                timestamp.hour %= 12;
            } else if (timestamp.hour != 12) {
                timestamp.hour += 12;
            }
            return true;
        }

        using FieldSlice = std::pair<std::size_t, std::size_t>;

        struct FilenameTimestampFields {
            std::size_t yearOffset = 0;
            std::size_t monthOffset = 0;
            std::size_t dayOffset = 0;
            std::size_t hourOffset = 0;
            std::size_t hourLength = 0;
            std::size_t minuteOffset = 0;
        };

        std::optional<SnapshotTimestamp> parseFields(const std::string_view text,
                                                     const std::array<FieldSlice, 6>& fields,
                                                     const bool dayFirst) {
            std::array<int, 6> values{};
            for (std::size_t index = 0; index < fields.size(); ++index) {
                const auto [offset, length] = fields[index];
                if (length == 0) {
                    continue;
                }
                if (offset + length > text.size()) {
                    return std::nullopt;
                }
                const auto value = parsePart(text.substr(offset, length));
                if (!value) {
                    return std::nullopt;
                }
                values[index] = *value;
            }
            SnapshotTimestamp timestamp;
            if (dayFirst) {
                timestamp.day = values[0];
                timestamp.month = values[1];
                timestamp.year = values[2];
            } else {
                timestamp.year = values[0];
                timestamp.month = values[1];
                timestamp.day = values[2];
            }
            timestamp.hour = values[3];
            timestamp.minute = values[4];
            timestamp.second = values[5];
            return validTimestamp(timestamp) ? std::optional{timestamp} : std::nullopt;
        }

        std::optional<SnapshotTimestamp> parseExactTimestamp(const std::string_view text) {
            static constexpr std::array<FieldSlice, 6> dateTimeFields{
                FieldSlice{0, 4},  FieldSlice{5, 2},  FieldSlice{8, 2},
                FieldSlice{11, 2}, FieldSlice{14, 2}, FieldSlice{17, 2}};
            static constexpr std::array<FieldSlice, 6> dayFirstDateTimeFields{
                FieldSlice{0, 2},  FieldSlice{3, 2},  FieldSlice{6, 4},
                FieldSlice{11, 2}, FieldSlice{14, 2}, FieldSlice{17, 2}};
            static constexpr std::array<FieldSlice, 6> monthFirstDateTimeFields{
                FieldSlice{3, 2},  FieldSlice{0, 2},  FieldSlice{6, 4},
                FieldSlice{11, 2}, FieldSlice{14, 2}, FieldSlice{17, 2}};
            static constexpr std::array<FieldSlice, 6> dayFirstMinuteFields{
                FieldSlice{0, 2},  FieldSlice{3, 2},  FieldSlice{6, 4},
                FieldSlice{11, 2}, FieldSlice{14, 2}, FieldSlice{0, 0}};
            static constexpr std::array<FieldSlice, 6> dateFields{
                FieldSlice{0, 4}, FieldSlice{5, 2}, FieldSlice{8, 2},
                FieldSlice{0, 0}, FieldSlice{0, 0}, FieldSlice{0, 0}};
            static constexpr std::array<FieldSlice, 6> dayFirstDateFields{
                FieldSlice{0, 2}, FieldSlice{3, 2}, FieldSlice{6, 4},
                FieldSlice{0, 0}, FieldSlice{0, 0}, FieldSlice{0, 0}};
            static constexpr std::array<FieldSlice, 6> monthFirstDateFields{
                FieldSlice{3, 2}, FieldSlice{0, 2}, FieldSlice{6, 4},
                FieldSlice{0, 0}, FieldSlice{0, 0}, FieldSlice{0, 0}};

            if (text.size() == 19 && text[4] == '-' && text[7] == '-' &&
                (text[10] == 'T' || text[10] == ' ') && text[13] == ':' && text[16] == ':') {
                return parseFields(text, dateTimeFields, false);
            }
            if (text.size() == 19 && text[2] == '/' && text[5] == '/' && text[10] == ' ' &&
                text[13] == ':' && text[16] == ':') {
                if (const auto dayFirst = parseFields(text, dayFirstDateTimeFields, true)) {
                    return dayFirst;
                }
                return parseFields(text, monthFirstDateTimeFields, true);
            }
            if (text.size() == 16 && text[2] == '/' && text[5] == '/' && text[10] == ' ' &&
                text[13] == ':') {
                return parseFields(text, dayFirstMinuteFields, true);
            }
            if (text.size() != 10) {
                return std::nullopt;
            }
            if (text[4] == '-' && text[7] == '-') {
                return parseFields(text, dateFields, false);
            }
            if ((text[2] == '-' && text[5] == '-') || (text[2] == '/' && text[5] == '/')) {
                if (const auto dayFirst = parseFields(text, dayFirstDateFields, true)) {
                    return dayFirst;
                }
                if (text[2] == '/') {
                    return parseFields(text, monthFirstDateFields, true);
                }
            }
            return std::nullopt;
        }

        std::optional<SnapshotTimestamp> filenameTimestampAt(const std::string_view text) {
            const auto parseTimestamp = [&](const FilenameTimestampFields& fields) {
                const auto year = parsePart(text.substr(fields.yearOffset, 4));
                const auto month = parsePart(text.substr(fields.monthOffset, 2));
                const auto day = parsePart(text.substr(fields.dayOffset, 2));
                const auto hour = parsePart(text.substr(fields.hourOffset, fields.hourLength));
                const auto minute = parsePart(text.substr(fields.minuteOffset, 2));
                if (!year || !month || !day || !hour || !minute) {
                    return std::optional<SnapshotTimestamp>{};
                }
                SnapshotTimestamp timestamp{*year, *month, *day, *hour, *minute, 0};
                return validTimestamp(timestamp) ? std::optional{timestamp}
                                                 : std::optional<SnapshotTimestamp>{};
            };

            if (text.size() >= 17 && text[2] == '-' && text[5] == '-' &&
                (text[10] == '_' || text[10] == '-' || text[10] == ' ')) {
                auto timestamp = parseTimestamp({6, 3, 0, 11, 2, 13});
                if (timestamp && applyMeridiem(*timestamp, std::string{text.substr(15, 2)})) {
                    return timestamp;
                }
            }

            if (text.size() >= 19 && text[2] == '-' && text[5] == '-' &&
                (text[10] == ' ' || text[10] == '_')) {
                const std::size_t hourLength = text[12] == ':' ? 1 : 2;
                const auto colon = 11 + hourLength;
                const auto minute = colon + 1;
                auto marker = minute + 2;
                if (text.size() > marker && (text[marker] == ' ' || text[marker] == '_')) {
                    ++marker;
                }
                if (text.size() >= marker + 2 && text[colon] == ':') {
                    auto timestamp = parseTimestamp({6, 3, 0, 11, hourLength, minute});
                    if (timestamp &&
                        applyMeridiem(*timestamp, std::string{text.substr(marker, 2)})) {
                        return timestamp;
                    }
                }
            }

            const auto dateSeparator = [](const char ch) {
                return ch == '-' || ch == '_' || ch == '/';
            };
            if (text.size() >= 10 && dateSeparator(text[2]) && dateSeparator(text[5])) {
                const auto day = parsePart(text.substr(0, 2));
                const auto month = parsePart(text.substr(3, 2));
                const auto year = parsePart(text.substr(6, 4));
                if (day && month && year) {
                    SnapshotTimestamp timestamp{*year, *month, *day, 0, 0, 0};
                    if (validTimestamp(timestamp)) {
                        return timestamp;
                    }
                }
            }
            if (text.size() >= 10 && dateSeparator(text[4]) && dateSeparator(text[7])) {
                const auto year = parsePart(text.substr(0, 4));
                const auto month = parsePart(text.substr(5, 2));
                const auto day = parsePart(text.substr(8, 2));
                if (year && month && day) {
                    SnapshotTimestamp timestamp{*year, *month, *day, 0, 0, 0};
                    if (validTimestamp(timestamp)) {
                        return timestamp;
                    }
                }
            }
            if (text.size() >= 16 && dateSeparator(text[4]) && dateSeparator(text[7]) &&
                (text[10] == ' ' || text[10] == 'T')) {
                const std::size_t hourLength = (text[12] == ':' || text[12] == '.') ? 1 : 2;
                const auto separator = 11 + hourLength;
                const auto minute = separator + 1;
                if (text.size() >= minute + 2 &&
                    (text[separator] == ':' || text[separator] == '.')) {
                    return parseTimestamp({0, 5, 8, 11, hourLength, minute});
                }
            }
            return std::nullopt;
        }

        std::optional<SnapshotTimestamp> filenameTimestamp(const std::string& value) {
            for (std::size_t index = 0; index < value.size(); ++index) {
                if (std::isdigit(static_cast<unsigned char>(value[index])) != 0) {
                    if (auto timestamp =
                            filenameTimestampAt(std::string_view{value}.substr(index))) {
                        return timestamp;
                    }
                }
            }
            return std::nullopt;
        }

        std::string timestampKey(const SnapshotTimestamp& timestamp) {
            std::array<char, 15> buffer{};
            std::snprintf(buffer.data(), buffer.size(), "%04d%02d%02d%02d%02d%02d", timestamp.year,
                          timestamp.month, timestamp.day, timestamp.hour, timestamp.minute,
                          timestamp.second);
            return buffer.data();
        }

        std::optional<std::string> snapshotKeyForField(const std::string& value) {
            if (const auto timestamp = parseExactTimestamp(value)) {
                return timestampKey(*timestamp);
            }
            return std::nullopt;
        }

        std::optional<std::string> snapshotKeyForFilename(const std::string& value) {
            if (const auto timestamp = filenameTimestamp(value)) {
                return timestampKey(*timestamp);
            }
            return std::nullopt;
        }

        std::optional<std::string> snapshotKey(const SsaImportPolicy::Values& values) {
            if (const auto filename = valueFor(values, "arquivo_origem"); !filename.empty()) {
                if (auto key = snapshotKeyForFilename(filename)) {
                    return key;
                }
            }
            static constexpr std::array<std::string_view, 3> keys{
                "data_planilha", "data_criacao_arquivo", "data_arquivo_origem"};
            for (const auto key : keys) {
                const auto value = valueFor(values, key);
                if (!value.empty()) {
                    if (auto snapshot = snapshotKeyForField(value)) {
                        return snapshot;
                    }
                }
            }
            return snapshotKeyForField(valueFor(values, "data_cadastro"));
        }

        bool isIndicatorField(const std::string_view key) {
            static constexpr std::array<std::string_view, 40> indicators{
                "prazo_limite",
                "status_execucao_prazo",
                "tempo_disponivel",
                "data_limite",
                "tempo_excedido",
                "desde",
                "desde_1",
                "desde_2",
                "ate",
                "ate_1",
                "ate_2",
                "tempo_total",
                "total_tempo_tpe_planejado",
                "total_tempo_tex_planejado",
                "total_tempo_tpo_planejado",
                "total_horas_programadas",
                "total_tempo_tpe_executada",
                "total_tempo_tex_executada",
                "total_tempo_tpo_executada",
                "semana_executada",
                "num_reprogramacoes",
                "total_de_reprogramacoes",
                "execucao_parcial",
                "parciais",
                "situacao_da_parcial",
                "anomalia",
                "registros_espera",
                "situacao_espera",
                "num_reprobaciones",
                "numero_desvios",
                "situacao_de_desvio",
                "justificativa",
                "data_inicio_programada",
                "data_programacao",
                "data_reprogramacao",
                "semana_programada",
                "responsavel_programacao",
                "responsavel_execucao",
                "execucao_simples",
                "descricao_execucao"};
            return std::ranges::find(indicators, key) != indicators.end() ||
                   key == "data_inicio_reprogramada" || key == "situacao_reprogramacao" ||
                   key == "executado" || key == "concluido";
        }

        bool isTransientField(const std::string_view key) {
            return key == "data_criacao_arquivo";
        }

        bool differsInPersistedValues(const SsaImportPolicy::Values& left,
                                      const SsaImportPolicy::Values& right) {
            for (const auto& [key, value] : left) {
                if (!isTransientField(key) && valueFor(right, key) != trimWhitespace(value)) {
                    return true;
                }
            }
            for (const auto& [key, value] : right) {
                if (!isTransientField(key) && valueFor(left, key) != trimWhitespace(value)) {
                    return true;
                }
            }
            return false;
        }

        bool isConflictField(const std::string_view key) {
            return key != "numero_ssa" && key != "arquivo_origem" && key != "data_planilha" &&
                   key != "data_criacao_arquivo" && key != "data_arquivo_origem" &&
                   !isIndicatorField(key);
        }

        int sourceProfilePriority(const SsaImportPolicy::Values& values) {
            switch (SsaImportPolicy::classifySourceProfile(valueFor(values, "arquivo_origem"))) {
            case SsaImportPolicy::SourceProfile::Executadas:
                return 3;
            case SsaImportPolicy::SourceProfile::DerivadasRelacionadas:
                return 2;
            case SsaImportPolicy::SourceProfile::Desvios:
                return 1;
            case SsaImportPolicy::SourceProfile::Geral:
                return 0;
            }
            return 0;
        }

    } // namespace

    std::string SsaImportPolicy::normalizeNumber(const std::string& value) {
        auto text = trimWhitespace(value);
        if (text.ends_with(".0")) {
            text.resize(text.size() - 2);
        }
        text.erase(std::remove_if(text.begin(), text.end(),
                                  [](const unsigned char ch) { return std::isspace(ch) != 0; }),
                   text.end());
        if (text.size() == 10 && text[4] == '-') {
            text.erase(4, 1);
        }
        if (text.size() != 9 || !std::ranges::all_of(text, [](const unsigned char ch) {
                return std::isdigit(ch) != 0;
            })) {
            return {};
        }
        return text;
    }

    std::string SsaImportPolicy::normalizeDeviationCount(const std::string& value) {
        auto normalized = trimWhitespace(value);
        std::ranges::transform(normalized, normalized.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        const auto integerLiteral = [](const std::string_view candidate) {
            const auto separator = candidate.find_first_of(".,");
            const auto digitsOnly = [](const std::string_view text) {
                return !text.empty() && std::ranges::all_of(text, [](const unsigned char ch) {
                    return std::isdigit(ch) != 0;
                });
            };
            if (separator == std::string_view::npos) {
                return digitsOnly(candidate) ? std::string{candidate} : std::string{};
            }
            if (separator == 0 || separator + 1 >= candidate.size() ||
                !digitsOnly(candidate.substr(0, separator)) ||
                !digitsOnly(candidate.substr(separator + 1)) ||
                !std::ranges::all_of(candidate.substr(separator + 1),
                                     [](const unsigned char ch) { return ch == '0'; })) {
                return std::string{};
            }
            return std::string{candidate.substr(0, separator)};
        };
        if (const auto numeric = integerLiteral(normalized); !numeric.empty()) {
            return numeric;
        }
        if (normalized.starts_with("desvio") || normalized.starts_with("desvios")) {
            auto suffix =
                std::string_view{normalized}.substr(normalized.starts_with("desvios") ? 7 : 6);
            while (!suffix.empty() && std::isspace(static_cast<unsigned char>(suffix.front()))) {
                suffix.remove_prefix(1);
            }
            if (!suffix.empty() &&
                (suffix.front() == '#' || suffix.front() == ':' || suffix.front() == '-')) {
                suffix.remove_prefix(1);
                while (!suffix.empty() &&
                       std::isspace(static_cast<unsigned char>(suffix.front()))) {
                    suffix.remove_prefix(1);
                }
            }
            if (const auto numeric = integerLiteral(suffix); !numeric.empty()) {
                return numeric;
            }
        }
        if (normalized == "sem desvio" || normalized == "sem desvios" || normalized == "nenhum" ||
            normalized == "nao" || normalized == "0 sem desvio" || normalized == "0 - sem desvio" ||
            normalized == "sem desvio (0)") {
            return "0";
        }
        return {};
    }

    std::string SsaImportPolicy::normalizeDateText(const std::string& value) {
        const auto text = trimWhitespace(value);
        const auto normalized = normalizeSnapshotTimestamp(text);
        if (normalized.empty()) {
            return {};
        }
        const bool hasIsoDatePrefix = text.size() >= 10 && text[4] == '-' && text[7] == '-';
        const bool isIsoDate = text.size() == 10;
        const bool isIsoDateTime = text.size() == 19 && (text[10] == ' ' || text[10] == 'T');
        return hasIsoDatePrefix && (isIsoDate || isIsoDateTime) ? text : normalized;
    }

    std::string SsaImportPolicy::normalizeSnapshotTimestamp(const std::string& value) {
        const auto timestamp = parseExactTimestamp(trimWhitespace(value));
        if (!timestamp) {
            return {};
        }
        std::array<char, 20> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02d %02d:%02d:%02d",
                      timestamp->year, timestamp->month, timestamp->day, timestamp->hour,
                      timestamp->minute, timestamp->second);
        return buffer.data();
    }

    std::string SsaImportPolicy::normalizeFilenameTimestamp(const std::string& filename) {
        const auto timestamp = filenameTimestamp(filename);
        if (!timestamp) {
            return {};
        }
        std::array<char, 20> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02d %02d:%02d:%02d",
                      timestamp->year, timestamp->month, timestamp->day, timestamp->hour,
                      timestamp->minute, timestamp->second);
        return buffer.data();
    }

    bool SsaImportPolicy::isTerminalStatus(const std::string& status) {
        return isTerminalStatusValue(status);
    }

    SsaImportPolicy::SourceProfile
    SsaImportPolicy::classifySourceProfile(const std::string& filename) {
        const auto normalized = lowercase(filename);
        if (normalized.find("executad") != std::string::npos) {
            return SourceProfile::Executadas;
        }
        if (normalized.find("derivad") != std::string::npos ||
            normalized.find("relacion") != std::string::npos) {
            return SourceProfile::DerivadasRelacionadas;
        }
        if (normalized.find("desvio") != std::string::npos) {
            return SourceProfile::Desvios;
        }
        return SourceProfile::Geral;
    }

    SsaImportPolicy::RowValidationIssue SsaImportPolicy::validateRow(const Values& row) {
        if (normalizeNumber(valueFor(row, "numero_ssa")).empty()) {
            return RowValidationIssue::InvalidNumber;
        }
        if (valueFor(row, "descricao_ssa").empty()) {
            return RowValidationIssue::MissingDescription;
        }
        const auto date = valueFor(row, "data_cadastro");
        if (date.empty()) {
            if (!isDateExemptStatus(valueFor(row, "situacao"))) {
                return RowValidationIssue::MissingDate;
            }
            if (!isValidWeek(valueFor(row, "semana_cadastro"))) {
                return RowValidationIssue::MissingWeek;
            }
        } else if (!parseExactTimestamp(date)) {
            return RowValidationIssue::InvalidDate;
        }
        for (const auto& [key, value] : row) {
            if (isDateField(key) && !valueFor(row, key).empty() && !parseExactTimestamp(value)) {
                return RowValidationIssue::InvalidDate;
            }
        }
        return RowValidationIssue::None;
    }

    bool SsaImportPolicy::isValidRow(const Values& row) {
        return validateRow(row) == RowValidationIssue::None;
    }

    SsaImportPolicy::MergeResult SsaImportPolicy::merge(const Values& existing,
                                                        const Values& incoming) {
        if (existing.empty()) {
            return {incoming, !incoming.empty()};
        }
        const auto existingSnapshot = snapshotKey(existing);
        const auto incomingSnapshot = snapshotKey(incoming);
        if (!existingSnapshot || !incomingSnapshot) {
            return {existing, false, existing != incoming};
        }
        const bool existingTerminal = isTerminalStatus(valueFor(existing, "situacao"));
        const bool incomingTerminal = isTerminalStatus(valueFor(incoming, "situacao"));
        const bool terminalPromotion = incomingTerminal && !existingTerminal;
        const bool olderTerminalPromotion =
            terminalPromotion && *incomingSnapshot < *existingSnapshot;
        if (*incomingSnapshot < *existingSnapshot && !terminalPromotion) {
            return {existing, false};
        }

        auto merged = existing;
        const bool terminal = existingTerminal;
        const bool equalSnapshot = *incomingSnapshot == *existingSnapshot;
        const bool incomingRicher = completenessScore(incoming) > completenessScore(existing);
        const bool incomingPreferredSource =
            sourceProfilePriority(incoming) > sourceProfilePriority(existing);
        bool conflict = false;
        for (const auto& [key, value] : incoming) {
            const auto normalized = trimWhitespace(value);
            const auto current = valueFor(existing, key);
            if (olderTerminalPromotion && key != "situacao") {
                continue;
            }
            if (isMetadataField(key)) {
                if (!normalized.empty() &&
                    (!equalSnapshot || current.empty() || normalized < current)) {
                    merged[key] = normalized;
                }
                continue;
            }
            if (equalSnapshot && isConflictField(key) && !normalized.empty() && !current.empty() &&
                normalized != current && !incomingRicher && !incomingPreferredSource &&
                !(key == "situacao" && incomingTerminal && !terminal)) {
                conflict = true;
            }
            if (normalized.empty() || (terminal && !isIndicatorField(key)) ||
                (equalSnapshot && key == "situacao" && !(incomingTerminal && !terminal))) {
                continue;
            }
            if (equalSnapshot && !current.empty() && !isIndicatorField(key) && !incomingRicher &&
                !incomingPreferredSource && !(key == "situacao" && incomingTerminal && !terminal)) {
                continue;
            }
            merged[key] = normalized;
        }
        return {merged, differsInPersistedValues(merged, existing), conflict};
    }

} // namespace ssa::domain
