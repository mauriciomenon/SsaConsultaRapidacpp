#include "domain/SsaImportPolicy.h"

#include "domain/AsciiText.h"
#include "domain/SsaTypes.h"
#include "domain/WhitespaceTrim.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <map>
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
            std::ranges::transform(value, value.begin(), ascii::toUpper);
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

        // Coded fields compare without case (Python parity): "ADM" and "adm" are one value.
        bool isCodedField(const std::string_view key) {
            static constexpr std::array<std::string_view, 7> coded{
                "situacao",           "setor_emissor",           "setor_executor",
                "localizacao_codigo", "grau_prioridade_emissao", "grau_prioridade_planejamento",
                "execucao_simples"};
            return std::ranges::find(coded, key) != coded.end();
        }

        std::string lowercase(std::string value);

        // Canonical content (Python parity, _canonical_row_content): decides between rows of
        // equal precedence by content only, never by the order they were read.
        std::string canonicalNumber(const std::string& text) {
            std::size_t index = 0;
            const bool negative = !text.empty() && text[0] == '-';
            if (negative || (!text.empty() && text[0] == '+')) {
                ++index;
            }
            std::string integer;
            std::string fraction;
            bool dot = false;
            bool digits = false;
            const auto exponentAt = text.find_first_of("eE", index);
            const auto mantissaEnd = exponentAt == std::string::npos ? text.size() : exponentAt;
            for (; index < mantissaEnd; ++index) {
                const char character = text[index];
                if (character >= '0' && character <= '9') {
                    (dot ? fraction : integer) += character;
                    digits = true;
                } else if (character == '.' && !dot) {
                    dot = true;
                } else {
                    return {};
                }
            }
            if (!digits) {
                return {};
            }
            if (exponentAt != std::string::npos) {
                // Decimal exponent (Python Decimal("1e3") == 1000), shifted without floating
                // point; an extreme exponent stays text.
                std::string_view exponentText{text};
                exponentText.remove_prefix(exponentAt + 1);
                const bool negativeExponent = exponentText.starts_with('-');
                if (negativeExponent || exponentText.starts_with('+')) {
                    exponentText.remove_prefix(1);
                }
                if (exponentText.empty() || exponentText.size() > 3 ||
                    !std::ranges::all_of(exponentText,
                                         [](const char c) { return c >= '0' && c <= '9'; })) {
                    return {};
                }
                int exponent = 0;
                for (const char c : exponentText) {
                    exponent = exponent * 10 + (c - '0');
                }
                if (exponent > 308) {
                    return {};
                }
                const auto all = integer + fraction;
                const auto point = static_cast<long long>(integer.size()) +
                                   (negativeExponent ? -exponent : exponent);
                if (point <= 0) {
                    integer.clear();
                    fraction = std::string(static_cast<std::size_t>(-point), '0') + all;
                } else if (static_cast<std::size_t>(point) >= all.size()) {
                    integer = all + std::string(static_cast<std::size_t>(point) - all.size(), '0');
                    fraction.clear();
                } else {
                    integer = all.substr(0, static_cast<std::size_t>(point));
                    fraction = all.substr(static_cast<std::size_t>(point));
                }
            }
            integer.erase(0, std::min(integer.find_first_not_of('0'), integer.size()));
            if (integer.empty()) {
                integer = "0";
            }
            while (!fraction.empty() && fraction.back() == '0') {
                fraction.pop_back();
            }
            const bool zero = integer == "0" && fraction.empty();
            return (negative && !zero ? "-" : "") + integer +
                   (fraction.empty() ? "" : "." + fraction);
        }

        std::map<std::string, std::string> canonicalRow(const SsaImportPolicy::Values& values) {
            std::map<std::string, std::string> canonical;
            for (const auto& [key, value] : values) {
                const auto text = trimWhitespace(value);
                if (key == "id" || text.empty()) {
                    continue;
                }
                if (auto number = canonicalNumber(text); !number.empty()) {
                    canonical[key] = std::move(number);
                } else if (auto date = SsaImportPolicy::normalizeSnapshotTimestamp(text);
                           !date.empty()) {
                    canonical[key] = std::move(date);
                } else {
                    canonical[key] = isCodedField(key) ? lowercase(text) : text;
                }
            }
            return canonical;
        }

        // json.dumps(sort_keys=True, ensure_ascii=True, separators=(",", ":")).
        void appendJsonString(std::string& out, const std::string& text) {
            static constexpr std::string_view kHex = "0123456789abcdef";
            const auto escape = [&out](const std::uint32_t codePoint) {
                out += "\\u";
                for (int shift = 12; shift >= 0; shift -= 4) {
                    out += kHex[(codePoint >> shift) & 0xFU];
                }
            };
            out += '"';
            for (std::size_t index = 0; index < text.size();) {
                const auto byte = static_cast<unsigned char>(text[index]);
                if (byte < 0x80U) {
                    switch (byte) {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    case '\b':
                        out += "\\b";
                        break;
                    case '\f':
                        out += "\\f";
                        break;
                    default:
                        if (byte < 0x20U || byte == 0x7FU) { // ensure_ascii escapes DEL too
                            escape(byte);
                        } else {
                            out += static_cast<char>(byte);
                        }
                    }
                    ++index;
                    continue;
                }
                const std::size_t length = byte >= 0xF0U ? 4 : byte >= 0xE0U ? 3 : 2;
                std::uint32_t codePoint = byte & (length == 4   ? 0x07U
                                                  : length == 3 ? 0x0FU
                                                                : 0x1FU);
                for (std::size_t offset = 1; offset < length && index + offset < text.size();
                     ++offset) {
                    codePoint = (codePoint << 6) |
                                (static_cast<unsigned char>(text[index + offset]) & 0x3FU);
                }
                if (codePoint > 0xFFFFU) {
                    codePoint -= 0x10000U;
                    escape(0xD800U | (codePoint >> 10));
                    escape(0xDC00U | (codePoint & 0x3FFU));
                } else {
                    escape(codePoint);
                }
                index += length;
            }
            out += '"';
        }

        std::string canonicalJson(const std::map<std::string, std::string>& row) {
            std::string json = "{";
            for (const auto& [key, value] : row) {
                if (json.size() > 1) {
                    json += ',';
                }
                appendJsonString(json, key);
                json += ':';
                appendJsonString(json, value);
            }
            return json + "}";
        }

        // More non-empty fields wins; then the larger canonical serialization.
        bool contentBeats(const std::map<std::string, std::string>& candidate,
                          const std::map<std::string, std::string>& holder) {
            if (candidate.size() != holder.size()) {
                return candidate.size() > holder.size();
            }
            return canonicalJson(candidate) > canonicalJson(holder);
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
            const auto yearWeek = parseIsoYearWeek(normalized);
            return yearWeek.has_value() && *yearWeek >= 198001 && *yearWeek <= 205053;
        }

        std::string lowercase(std::string value) {
            std::ranges::transform(value, value.begin(), ascii::toLower);
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
            // ISO date with hour before the date-only form, which would otherwise hide it.
            if (text.size() >= 16 && dateSeparator(text[4]) && dateSeparator(text[7]) &&
                (text[10] == ' ' || text[10] == 'T')) {
                const std::size_t hourLength = (text[12] == ':' || text[12] == '.') ? 1 : 2;
                const auto separator = 11 + hourLength;
                const auto minute = separator + 1;
                const bool minuteEnds =
                    text.size() <= minute + 2 || !ascii::isDigit(text[minute + 2]);
                if (text.size() >= minute + 2 &&
                    (text[separator] == ':' || text[separator] == '.') && minuteEnds) {
                    if (auto timestamp = parseTimestamp({0, 5, 8, 11, hourLength, minute})) {
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
            return std::nullopt;
        }

        std::optional<SnapshotTimestamp> filenameTimestamp(const std::string& value) {
            for (std::size_t index = 0; index < value.size(); ++index) {
                if (ascii::isDigit(value[index])) {
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

        // Folds UTF-8 Latin-1 letters (U+00C0..U+00FF, two bytes 0xC3 xx) to their ASCII
        // base: SAM exports the same text with and without Portuguese accents.
        std::string withoutAccents(const std::string_view text) {
            static constexpr std::string_view kBase =
                "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
            std::string folded;
            folded.reserve(text.size());
            for (std::size_t index = 0; index < text.size(); ++index) {
                const auto lead = static_cast<unsigned char>(text[index]);
                const auto next =
                    index + 1 < text.size() ? static_cast<unsigned char>(text[index + 1]) : 0U;
                if (lead == 0xC3U && next >= 0x80U && next <= 0xBFU) {
                    folded += kBase[next - 0x80U];
                    ++index;
                } else {
                    folded += static_cast<char>(lead);
                }
            }
            return folded;
        }

        bool hasNonAscii(const std::string_view text) {
            return std::ranges::any_of(text, [](const char character) {
                return static_cast<unsigned char>(character) > 0x7FU;
            });
        }

        // Python file date: the date in the filename, else the modification time, else the
        // creation time (a staged copy carries a fresh one, so it comes last).
        std::optional<std::string> fileDateKey(const SsaImportPolicy::Values& values) {
            if (const auto filename = valueFor(values, "arquivo_origem"); !filename.empty()) {
                if (auto key = snapshotKeyForFilename(filename)) {
                    return key;
                }
            }
            for (const auto key : {"data_arquivo_origem", "data_criacao_arquivo"}) {
                if (auto snapshot = snapshotKeyForField(valueFor(values, key))) {
                    return snapshot;
                }
            }
            return std::nullopt;
        }

        // Python precedence tuple (data_planilha, file date): the sheet date decides; the file
        // date breaks a tie in merge(). The registration date is the last fallback for a row
        // with no other date and never breaks a tie.
        std::optional<std::string> snapshotKey(const SsaImportPolicy::Values& values) {
            if (auto sheet = snapshotKeyForField(valueFor(values, "data_planilha"))) {
                return sheet;
            }
            if (auto file = fileDateKey(values)) {
                return file;
            }
            return snapshotKeyForField(valueFor(values, "data_cadastro"));
        }

        // Tie-break of an equal snapshot: the file date. The separator sorts before any
        // digit, so a missing file date sorts before a present one: the order is total and
        // the result never depends on row order. Rows still equal are resolved by content.
        std::string snapshotTieKey(const SsaImportPolicy::Values& values) {
            return " " + fileDateKey(values).value_or("");
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

        // Python parity of value equality: dates compare as canonical timestamps
        // ("01/01/2026" == "2026-01-01 00:00:00"), numbers as exact canonical decimals
        // ("1.0" == "1", no floating point), coded fields without case.
        bool sameValue(const std::string_view key, const std::string& left,
                       const std::string& right) {
            if (left == right) {
                return true;
            }
            if (left.empty() || right.empty()) {
                return false;
            }
            if (isCodedField(key) && uppercase(left) == uppercase(right)) {
                return true;
            }
            // Numeric spelling ("1.0" == "1", "1e3" == "1000") but never a code with leading
            // zeros or an explicit sign: "007" and "7" stay different texts.
            const auto plainNumber = [](const std::string& text) {
                const auto digits = text.starts_with('-') ? std::string_view{text}.substr(1)
                                                          : std::string_view{text};
                const bool leadingZero = digits.size() > 1 && digits[0] == '0' && digits[1] != '.';
                return !text.starts_with('+') && !leadingZero;
            };
            if (const auto number = canonicalNumber(left); !number.empty() && plainNumber(left) &&
                                                           plainNumber(right) &&
                                                           number == canonicalNumber(right)) {
                return true;
            }
            if (isDateField(key)) {
                const auto date = SsaImportPolicy::normalizeSnapshotTimestamp(left);
                return !date.empty() && date == SsaImportPolicy::normalizeSnapshotTimestamp(right);
            }
            return false;
        }

        bool differsInPersistedValues(const SsaImportPolicy::Values& left,
                                      const SsaImportPolicy::Values& right,
                                      const bool ignoreMetadata = false) {
            const auto ignored = [ignoreMetadata](const std::string_view key) {
                return isTransientField(key) || (ignoreMetadata && isMetadataField(key));
            };
            const auto differs = [&ignored](const SsaImportPolicy::Values& values,
                                            const SsaImportPolicy::Values& other) {
                return std::ranges::any_of(values, [&](const auto& entry) {
                    return !ignored(entry.first) &&
                           !sameValue(entry.first, valueFor(other, entry.first),
                                      trimWhitespace(entry.second));
                });
            };
            return differs(left, right) || differs(right, left);
        }

    } // namespace

    std::string SsaImportPolicy::normalizeNumber(const std::string& value) {
        auto text = trimWhitespace(value);
        if (text.ends_with(".0")) {
            text.resize(text.size() - 2);
        }
        text.erase(std::remove_if(text.begin(), text.end(), ascii::isSpace), text.end());
        if (text.size() == 10 && text[4] == '-') {
            text.erase(4, 1);
        }
        if (text.size() != 9 || !std::ranges::all_of(text, ascii::isDigit)) {
            return {};
        }
        return text;
    }

    std::string SsaImportPolicy::normalizeDeviationCount(const std::string& value) {
        auto normalized = trimWhitespace(value);
        std::ranges::transform(normalized, normalized.begin(), ascii::toLower);
        const auto integerLiteral = [](const std::string_view candidate) {
            const auto separator = candidate.find_first_of(".,");
            const auto digitsOnly = [](const std::string_view text) {
                return !text.empty() && std::ranges::all_of(text, ascii::isDigit);
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
            while (!suffix.empty() && ascii::isSpace(suffix.front())) {
                suffix.remove_prefix(1);
            }
            // "Desvio #N" labels one row of the deviation report (one row per deviation), it is
            // not the count; storing it as a count overwrote real counts with 1.
            if (!suffix.empty() && suffix.front() == '#') {
                return {};
            }
            if (!suffix.empty() && (suffix.front() == ':' || suffix.front() == '-')) {
                suffix.remove_prefix(1);
                while (!suffix.empty() && ascii::isSpace(suffix.front())) {
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
        // Storage form: ISO input is kept as written, legacy dd/mm/yyyy becomes ISO. Equality
        // of two dates is decided by sameValue(), which compares canonical timestamps.
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

    std::string SsaImportPolicy::normalizeDateValue(const ColumnType type,
                                                    const std::string& value) {
        return type == ColumnType::DateTimeText ? normalizeSnapshotTimestamp(value)
                                                : normalizeDateText(value);
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

    std::string SsaImportPolicy::precedenceKey(const Values& row) {
        const auto snapshot = snapshotKey(row);
        return snapshot ? *snapshot + snapshotTieKey(row) : std::string{};
    }

    SsaImportPolicy::ContentKey SsaImportPolicy::contentKey(const Values& row) {
        const auto canonical = canonicalRow(row);
        return {canonical.size(), canonicalJson(canonical)};
    }

    bool SsaImportPolicy::isOpenAfterTerminal(const Values& open, const Values& terminal) {
        if (isTerminalStatus(valueFor(open, "situacao")) ||
            !isTerminalStatus(valueFor(terminal, "situacao"))) {
            return false;
        }
        const auto openKey = precedenceKey(open);
        const auto terminalKey = precedenceKey(terminal);
        return !openKey.empty() && !terminalKey.empty() && openKey > terminalKey;
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
        const auto issueWeek = valueFor(row, "semana_cadastro");
        if (!issueWeek.empty() && !isValidWeek(issueWeek)) {
            return RowValidationIssue::MissingWeek;
        }
        if (date.empty()) {
            if (!isDateExemptStatus(valueFor(row, "situacao"))) {
                return RowValidationIssue::MissingDate;
            }
            if (issueWeek.empty()) {
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
        auto existingSnapshot = snapshotKey(existing);
        auto incomingSnapshot = snapshotKey(incoming);
        if (existingSnapshot && incomingSnapshot && *existingSnapshot == *incomingSnapshot) {
            *existingSnapshot += snapshotTieKey(existing);
            *incomingSnapshot += snapshotTieKey(incoming);
        }
        const bool existingTerminal = isTerminalStatus(valueFor(existing, "situacao"));
        const bool incomingTerminal = isTerminalStatus(valueFor(incoming, "situacao"));
        const bool openAfterTerminal =
            isOpenAfterTerminal(incoming, existing) || isOpenAfterTerminal(existing, incoming);
        // Python parity: a row without a report date never wins over a dated one, except a
        // terminal row over an open one; a dated row (or a tie without dates) updates an
        // undated one. Never a conflict.
        if (!incomingSnapshot && existingSnapshot) {
            if (!incomingTerminal || existingTerminal) {
                return {existing, false};
            }
            incomingSnapshot = ""; // sorts before any date: the older-terminal path below
        }
        if (!existingSnapshot) {
            existingSnapshot = "";
            incomingSnapshot = incomingSnapshot.value_or("0");
        }
        // A terminal row (STE/SCA) never reopens and wins whole over any open row, whatever the
        // dates, so the result does not depend on read order. An equal snapshot still runs
        // the content check below: two sources describing the same instant must agree.
        if (existingTerminal && *incomingSnapshot != *existingSnapshot) {
            return {.values = existing, .openAfterTerminal = openAfterTerminal};
        }
        const bool terminalPromotion = incomingTerminal && !existingTerminal;
        if (*incomingSnapshot < *existingSnapshot && !terminalPromotion) {
            return {existing, false};
        }

        auto merged = existing;
        const bool equalSnapshot = *incomingSnapshot == *existingSnapshot;
        bool repeatDiscarded = false;
        std::string firstDifference;
        if (equalSnapshot) {
            // Python parity: rows of equal precedence are resolved by content, never rejected.
            // The winner's values overwrite; the loser only fills empty fields. `situacao`
            // changes only by terminal promotion, which also makes the incoming row win.
            const auto existingContent = canonicalRow(existing);
            const auto incomingContent = canonicalRow(incoming);
            const bool identical = existingContent == incomingContent;
            const bool incomingWins =
                !identical && (terminalPromotion || contentBeats(incomingContent, existingContent));
            for (const auto& [key, value] : incoming) {
                const auto normalized = trimWhitespace(value);
                if (normalized.empty()) {
                    continue;
                }
                const auto current = valueFor(existing, key);
                if (current.empty()) {
                    merged[key] = normalized;
                    continue;
                }
                if (sameValue(key, normalized, current)) {
                    continue; // equal after canonical comparison: keep the stored spelling
                }
                if (key == "situacao") {
                    // The winning row carries its state; a terminal promotion always wins.
                    if (terminalPromotion || incomingWins) {
                        merged[key] = normalized;
                    }
                    continue;
                }
                if (!isMetadataField(key) && (firstDifference.empty() || key < firstDifference)) {
                    firstDifference = key; // smallest key: stable across hash-map order
                }
                if (incomingWins) {
                    merged[key] = normalized;
                }
            }
            // Journal every repeat whose business fields differ, whichever row won.
            repeatDiscarded = !identical && !firstDifference.empty();
        } else {
            for (const auto& [key, value] : incoming) {
                const auto normalized = trimWhitespace(value);
                if (normalized.empty()) {
                    continue;
                }
                if (sameValue(key, normalized, valueFor(existing, key))) {
                    continue; // same value in another spelling: keep the stored one
                }
                merged[key] = normalized;
            }
        }
        if (existingTerminal) {
            return {existing, false};
        }
        const bool changed = differsInPersistedValues(merged, existing);
        return {merged,
                changed,
                false,
                std::move(firstDifference),
                repeatDiscarded,
                changed && !differsInPersistedValues(merged, existing, true),
                openAfterTerminal};
    }

    std::string SsaImportPolicy::describeConflict(const Values& existing, const Values& incoming,
                                                  const MergeResult& result) {
        constexpr std::size_t kMaxValueLength = 60;
        const auto quoted = [](const Values& values, const std::string_view key,
                               const bool clip = true) {
            auto value = valueFor(values, key);
            if (clip && value.size() > kMaxValueLength) {
                value.resize(kMaxValueLength);
                value += "...";
            }
            return "\"" + value + "\"";
        };
        const std::string field = result.openAfterTerminal ? "situacao" : result.conflictField;
        auto text = std::string{result.openAfterTerminal ? "Import open row newer than terminal: "
                                : result.conflict ? "Import conflict: "
                                                  : "Import repeated SSA, resolved by content: "} +
                    "ssa=" + valueFor(incoming, "numero_ssa") + " field=" + field +
                    " existing=" + quoted(existing, field) +
                    " incoming=" + quoted(incoming, field) +
                    " existing_file=" + quoted(existing, "arquivo_origem", false) +
                    " incoming_file=" + quoted(incoming, "arquivo_origem", false);
        if (result.openAfterTerminal) {
            text += " existing_date=" + quoted(existing, "data_planilha", false) +
                    " incoming_date=" + quoted(incoming, "data_planilha", false);
        }
        return text;
    }

} // namespace ssa::domain
