#include "infra/import/XlsxWorkbookReader.h"

#include "infra/import/XlsxPackage.h"

#include <QByteArray>
#include <QDate>
#include <QDir>
#include <QString>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ssa::infra::importing {

    namespace {

        constexpr std::size_t kMaxWorksheetRows = 250'000;
        constexpr std::size_t kMaxWorksheetCells = 5'000'000;
        constexpr int kSpreadsheetColumnBase = 26;
        constexpr qsizetype kDefaultSharedStringReserve = 256;

        void throwIfImportCanceled(const std::stop_token& stopToken) {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "xlsx read canceled");
            }
        }

        QByteArray xmlBytes(const std::string& xml) {
            return QByteArray::fromRawData(xml.data(), static_cast<qsizetype>(xml.size()));
        }

        std::string toStdString(const QStringView value) {
            return value.toString().toStdString();
        }

        std::optional<int> columnIndexFromCellRef(const QStringView ref) {
            constexpr int kMaxImportedColumns = 512;
            int index = 0;
            bool hasLetters = false;
            for (const auto character : ref) {
                if (!character.isLetter()) {
                    break;
                }
                hasLetters = true;
                index = (index * kSpreadsheetColumnBase) +
                        (character.toUpper().unicode() - QChar{'A'}.unicode() + 1);
                if (index > kMaxImportedColumns) {
                    return std::nullopt;
                }
            }
            if (!hasLetters || index <= 0) {
                return std::nullopt;
            }
            return index - 1;
        }

        std::size_t absoluteColumnSlotsFromDimensionRef(const QStringView ref) {
            const auto separator = ref.indexOf(':');
            const auto lastCell =
                separator >= 0 && separator + 1 < ref.size() ? ref.sliced(separator + 1) : ref;
            const auto columnIndex = columnIndexFromCellRef(lastCell);
            return columnIndex ? static_cast<std::size_t>(*columnIndex + 1) : 0;
        }

        std::size_t absoluteRowSlotsFromDimensionRef(const QStringView ref) {
            const auto separator = ref.indexOf(':');
            const auto lastCell =
                separator >= 0 && separator + 1 < ref.size() ? ref.sliced(separator + 1) : ref;
            qsizetype digitStart = 0;
            while (digitStart < lastCell.size() && !lastCell[digitStart].isDigit()) {
                ++digitStart;
            }
            if (digitStart >= lastCell.size()) {
                return 0;
            }
            bool parsed = false;
            const auto rows = lastCell.sliced(digitStart).toULongLong(&parsed);
            return parsed ? static_cast<std::size_t>(rows) : 0;
        }

        std::optional<std::size_t> rowIndexFromRowRef(const QStringView ref) {
            if (ref.empty()) {
                return std::nullopt;
            }
            bool parsed = false;
            const auto row = ref.toULongLong(&parsed);
            if (!parsed || row == 0 || row > kMaxWorksheetRows) {
                throw std::runtime_error("xlsx row index out of supported range");
            }
            return static_cast<std::size_t>(row - 1);
        }

        class XmlReadBudget final {
          public:
            explicit XmlReadBudget(std::stop_token stopToken) : stopToken_(std::move(stopToken)) {}

            void record(const QXmlStreamReader& reader) {
                constexpr std::size_t kMaxXmlTokens = 20'000'000;
                constexpr int kMaxXmlDepth = 64;
                throwIfImportCanceled(stopToken_);
                ++tokenCount_;
                if (reader.isStartElement()) {
                    ++depth_;
                } else if (reader.isEndElement() && depth_ > 0) {
                    --depth_;
                }
                if (tokenCount_ > kMaxXmlTokens || depth_ > kMaxXmlDepth) {
                    throw std::runtime_error("xlsx XML payload too complex");
                }
            }

          private:
            std::stop_token stopToken_;
            std::size_t tokenCount_ = 0;
            int depth_ = 0;
        };

        class XmlTextBudget final {
          public:
            void record(const qsizetype characterCount) {
                constexpr qsizetype kMaxXmlTextCharacters = 134'217'728;
                totalCharacters_ += characterCount;
                if (totalCharacters_ > kMaxXmlTextCharacters) {
                    throw std::runtime_error("xlsx text payload too large");
                }
            }

          private:
            qsizetype totalCharacters_ = 0;
        };

        void recordXmlToken(const QXmlStreamReader& reader, XmlReadBudget& budget) {
            budget.record(reader);
        }

        void appendCharacters(QString& output, const QStringView text, XmlTextBudget& textBudget) {
            constexpr qsizetype kMaxCellTextLength = 32768;
            if (output.size() + text.size() > kMaxCellTextLength) {
                throw std::runtime_error("xlsx cell text too large");
            }
            textBudget.record(text.size());
            output += text;
        }

        void readTextElementInto(QXmlStreamReader& reader, XmlReadBudget& budget,
                                 XmlTextBudget& textBudget, QString& output) {
            while (!reader.atEnd() && !reader.hasError()) {
                reader.readNext();
                if (reader.atEnd()) {
                    break;
                }
                recordXmlToken(reader, budget);
                if (reader.isCharacters()) {
                    appendCharacters(output, reader.text(), textBudget);
                } else if (reader.isEndElement() && reader.name() == "t") {
                    break;
                }
            }
        }

        bool isBuiltInDateFormat(const unsigned formatId) {
            return (formatId >= 14 && formatId <= 22) || (formatId >= 27 && formatId <= 36) ||
                   (formatId >= 45 && formatId <= 47) || (formatId >= 50 && formatId <= 58);
        }

        bool isCustomDateFormat(const QStringView formatCode) {
            bool quoted = false;
            for (qsizetype index = 0; index < formatCode.size(); ++index) {
                const auto character = formatCode[index];
                if (character == '\\') {
                    ++index;
                    continue;
                }
                if (character == '"') {
                    quoted = !quoted;
                    continue;
                }
                if (quoted) {
                    continue;
                }
                if (character == '_' || character == '*') {
                    ++index;
                    continue;
                }
                if (character == '[') {
                    while (index < formatCode.size() && formatCode[index] != ']') {
                        ++index;
                    }
                    continue;
                }
                const auto lower = character.toLower();
                if (lower == 'y' || lower == 'd') {
                    return true;
                }
            }
            return false;
        }

        std::vector<bool> parseDateStyles(const std::string& xml,
                                          const std::stop_token& stopToken) {
            std::vector<unsigned> formatIds;
            std::unordered_set<unsigned> customDateFormats;
            if (xml.empty()) {
                return {};
            }
            QXmlStreamReader reader(xmlBytes(xml));
            XmlReadBudget budget(stopToken);
            bool inCellFormats = false;
            while (!reader.atEnd()) {
                reader.readNext();
                recordXmlToken(reader, budget);
                if (reader.isStartElement() && reader.name() == "numFmt") {
                    bool parsed = false;
                    const auto formatId = reader.attributes().value("numFmtId").toUInt(&parsed);
                    if (parsed && isCustomDateFormat(reader.attributes().value("formatCode"))) {
                        customDateFormats.insert(formatId);
                    }
                } else if (reader.isStartElement() && reader.name() == "cellXfs") {
                    inCellFormats = true;
                } else if (inCellFormats && reader.isStartElement() && reader.name() == "xf") {
                    bool parsed = false;
                    const auto formatId = reader.attributes().value("numFmtId").toUInt(&parsed);
                    formatIds.push_back(parsed ? formatId : 0);
                } else if (reader.isEndElement() && reader.name() == "cellXfs") {
                    inCellFormats = false;
                }
            }
            if (reader.hasError()) {
                throw std::runtime_error("cannot parse xlsx styles");
            }
            std::vector<bool> dateStyles;
            dateStyles.reserve(formatIds.size());
            for (const auto formatId : formatIds) {
                dateStyles.push_back(isBuiltInDateFormat(formatId) ||
                                     customDateFormats.contains(formatId));
            }
            return dateStyles;
        }

        bool usesDate1904(const std::string& workbookXml, const std::stop_token& stopToken) {
            QXmlStreamReader reader(xmlBytes(workbookXml));
            XmlReadBudget budget(stopToken);
            while (!reader.atEnd()) {
                reader.readNext();
                recordXmlToken(reader, budget);
                if (reader.isStartElement() && reader.name() == "workbookPr") {
                    const auto value = reader.attributes().value("date1904").toString().toLower();
                    return value == "1" || value == "true";
                }
            }
            if (reader.hasError()) {
                throw std::runtime_error("cannot parse xlsx workbook properties");
            }
            return false;
        }

        std::string excelDateValue(const QString& rawValue, const bool date1904) {
            bool parsed = false;
            const double serial = rawValue.toDouble(&parsed);
            if (!parsed || !std::isfinite(serial) || serial < 0.0) {
                throw std::runtime_error("invalid xlsx date serial");
            }
            auto wholeDays = static_cast<qint64>(std::floor(serial));
            auto seconds = static_cast<qint64>(
                std::llround((serial - static_cast<double>(wholeDays)) * 86'400.0));
            if (seconds == 86'400) {
                ++wholeDays;
                seconds = 0;
            }
            QDate baseDate = date1904 ? QDate{1904, 1, 1} : QDate{1899, 12, 31};
            if (!date1904 && wholeDays >= 60) {
                --wholeDays;
            }
            const auto date = baseDate.addDays(wholeDays);
            if (!date.isValid()) {
                throw std::runtime_error("xlsx date serial out of supported range");
            }
            const auto hours = seconds / 3'600;
            const auto minutes = (seconds % 3'600) / 60;
            const auto remainingSeconds = seconds % 60;
            return QStringLiteral("%1T%2:%3:%4")
                .arg(date.toString(QStringLiteral("yyyy-MM-dd")))
                .arg(hours, 2, 10, QChar{'0'})
                .arg(minutes, 2, 10, QChar{'0'})
                .arg(remainingSeconds, 2, 10, QChar{'0'})
                .toStdString();
        }

        std::string decodeCellValue(const QString& rawValue, const QStringView type,
                                    const std::vector<std::string>& sharedStrings,
                                    const bool dateStyle, const bool date1904) {
            if (rawValue.isEmpty()) {
                return {};
            }
            if (type == "s") {
                bool parsed = false;
                const auto index = rawValue.toInt(&parsed);
                if (parsed && index >= 0 &&
                    static_cast<std::size_t>(index) < sharedStrings.size()) {
                    return sharedStrings[static_cast<std::size_t>(index)];
                }
                return {};
            }
            if (dateStyle && (type.empty() || type == "n")) {
                return excelDateValue(rawValue, date1904);
            }
            return rawValue.toStdString();
        }

        struct CellRawValue {
            QString text;
            bool hasFormula = false;
            bool hasCachedValue = false;
        };

        CellRawValue readCellRawValue(QXmlStreamReader& reader, XmlReadBudget& budget,
                                      XmlTextBudget& textBudget) {
            CellRawValue value;
            while (!reader.atEnd() && !reader.hasError()) {
                reader.readNext();
                if (reader.atEnd()) {
                    break;
                }
                recordXmlToken(reader, budget);
                if (reader.isStartElement() && reader.name() == "f") {
                    value.hasFormula = true;
                } else if (reader.isStartElement() &&
                           (reader.name() == "v" || reader.name() == "t")) {
                    value.hasCachedValue = true;
                    while (!reader.atEnd() && !reader.hasError()) {
                        reader.readNext();
                        if (reader.atEnd()) {
                            break;
                        }
                        recordXmlToken(reader, budget);
                        if (reader.isCharacters()) {
                            appendCharacters(value.text, reader.text(), textBudget);
                        } else if (reader.isEndElement() &&
                                   (reader.name() == "v" || reader.name() == "t")) {
                            break;
                        }
                    }
                } else if (reader.isEndElement() && reader.name() == "c") {
                    break;
                }
            }
            return value;
        }

        std::string normalizeWorksheetTarget(const std::string& target) {
            QString path = QString::fromStdString(target);
            if (path.startsWith('/')) {
                path.remove(0, 1);
            } else if (!path.startsWith("xl/")) {
                path = "xl/" + path;
            }
            path = QDir::cleanPath(path);
            if (path == "." || path.startsWith("../") || path.contains("/../")) {
                throw std::runtime_error("unsafe xlsx worksheet target");
            }
            if (!path.startsWith("xl/worksheets/") || !path.endsWith(".xml")) {
                throw std::runtime_error("unexpected xlsx worksheet target");
            }
            return path.toStdString();
        }

        bool isRelationshipIdAttribute(const QXmlStreamAttribute& attribute) {
            return attribute.qualifiedName() == "r:id" ||
                   (attribute.name() == "id" &&
                    attribute.namespaceUri() ==
                        QStringLiteral(
                            "http://schemas.openxmlformats.org/officeDocument/2006/relationships"));
        }

        class SheetRowsParser final {
          public:
            SheetRowsParser(const std::string& xml, const std::vector<std::string>& sharedStrings,
                            const std::vector<bool>& dateStyles, const bool date1904,
                            std::stop_token stopToken)
                : reader_(xmlBytes(xml)), sharedStrings_(sharedStrings), dateStyles_(dateStyles),
                  date1904_(date1904), readBudget_(std::move(stopToken)) {}

            [[nodiscard]] std::vector<std::vector<std::string>> parse() {
                while (!reader_.atEnd()) {
                    reader_.readNext();
                    recordXmlToken(reader_, readBudget_);
                    if (reader_.isStartElement() && reader_.name() == "dimension") {
                        handleDimension();
                    } else if (reader_.isStartElement() && reader_.name() == "row") {
                        startRow();
                    } else if (inRow_ && reader_.isStartElement() && reader_.name() == "c") {
                        readCell();
                    } else if (reader_.isEndElement() && reader_.name() == "row") {
                        finishRow();
                    }
                }
                if (reader_.hasError()) {
                    throw std::runtime_error("cannot parse xlsx worksheet");
                }
                return rows_;
            }

          private:
            void handleDimension() {
                expectedColumns_ =
                    absoluteColumnSlotsFromDimensionRef(reader_.attributes().value("ref"));
                if (expectedColumns_ > 0) {
                    currentRow_.reserve(expectedColumns_);
                }
                const auto expectedRows =
                    absoluteRowSlotsFromDimensionRef(reader_.attributes().value("ref"));
                if (expectedRows > 0) {
                    rows_.reserve((std::min)(expectedRows, kMaxWorksheetRows));
                }
            }

            void startRow() {
                currentRow_.clear();
                if (expectedColumns_ > 0) {
                    currentRow_.reserve(expectedColumns_);
                }
                currentRowIndex_ = rowIndexFromRowRef(reader_.attributes().value("r"));
                nextColumnIndex_ = 0;
                inRow_ = true;
            }

            void readCell() {
                const auto attributes = reader_.attributes();
                const auto ref = attributes.value("r");
                const auto type = attributes.value("t");
                bool styleParsed = false;
                const auto styleIndex = attributes.value("s").toUInt(&styleParsed);
                const auto columnIndex = columnIndexFromCellRef(ref);
                const auto rawValue = readCellRawValue(reader_, readBudget_, textBudget_);
                if (rawValue.hasFormula && !rawValue.hasCachedValue) {
                    throw std::runtime_error("xlsx formula cell has no cached value");
                }
                if (!columnIndex && !ref.isEmpty()) {
                    return;
                }
                const auto resolvedColumn =
                    columnIndex ? static_cast<std::size_t>(*columnIndex) : nextColumnIndex_;
                currentRow_.resize(std::max<std::size_t>(currentRow_.size(), resolvedColumn + 1));
                const bool dateStyle =
                    styleParsed && styleIndex < dateStyles_.size() && dateStyles_[styleIndex];
                currentRow_[resolvedColumn] =
                    decodeCellValue(rawValue.text, type, sharedStrings_, dateStyle, date1904_);
                nextColumnIndex_ = resolvedColumn + 1;
                ++totalCells_;
                if (totalCells_ > kMaxWorksheetCells) {
                    throw std::runtime_error("xlsx worksheet has too many cells");
                }
            }

            void finishRow() {
                if (currentRowIndex_) {
                    rows_.resize((std::max)(rows_.size(), *currentRowIndex_ + 1));
                    rows_[*currentRowIndex_] = std::move(currentRow_);
                } else {
                    rows_.push_back(std::move(currentRow_));
                }
                currentRow_ = {};
                if (rows_.size() > kMaxWorksheetRows) {
                    throw std::runtime_error("xlsx worksheet has too many rows");
                }
                inRow_ = false;
                currentRowIndex_.reset();
            }

            QXmlStreamReader reader_;
            const std::vector<std::string>& sharedStrings_;
            const std::vector<bool>& dateStyles_;
            bool date1904_ = false;
            std::vector<std::vector<std::string>> rows_;
            std::vector<std::string> currentRow_;
            bool inRow_ = false;
            std::optional<std::size_t> currentRowIndex_ = std::nullopt;
            std::size_t nextColumnIndex_ = 0;
            std::size_t expectedColumns_ = 0;
            std::size_t totalCells_ = 0;
            XmlReadBudget readBudget_;
            XmlTextBudget textBudget_;
        };

    } // namespace

    SpreadsheetTable XlsxWorkbookReader::readFirstSheet(const std::filesystem::path& filePath,
                                                        const std::stop_token& stopToken) {
        throwIfImportCanceled(stopToken);
        XlsxPackage package(filePath);
        const auto workbook = package.textEntry("xl/workbook.xml", true, stopToken);
        const auto relationshipId = relationshipIdForFirstWorkbookSheet(workbook, stopToken);
        throwIfImportCanceled(stopToken);
        const auto relationships = package.textEntry("xl/_rels/workbook.xml.rels", true, stopToken);
        const auto worksheetEntry =
            worksheetEntryForRelationship({relationships, relationshipId}, stopToken);
        const auto sharedStrings = parseSharedStrings(
            package.textEntry("xl/sharedStrings.xml", false, stopToken), stopToken);
        const auto dateStyles =
            parseDateStyles(package.textEntry("xl/styles.xml", false, stopToken), stopToken);
        const bool date1904 = usesDate1904(workbook, stopToken);
        throwIfImportCanceled(stopToken);
        const auto sheetXml = package.textEntry(worksheetEntry, true, stopToken);
        SpreadsheetTable table;
        table.sourcePath = filePath;
        table.rows = parseSheetRows(sheetXml, sharedStrings, dateStyles, date1904, stopToken);
        return table;
    }

    std::vector<std::string>
    XlsxWorkbookReader::parseSharedStrings(const std::string& xml,
                                           const std::stop_token& stopToken) {
        std::vector<std::string> values;
        if (xml.empty()) {
            return values;
        }
        QXmlStreamReader reader(xmlBytes(xml));
        QString current;
        current.reserve(kDefaultSharedStringReserve);
        bool inString = false;
        int phoneticDepth = 0;
        XmlReadBudget budget(stopToken);
        XmlTextBudget textBudget;
        while (!reader.atEnd()) {
            reader.readNext();
            recordXmlToken(reader, budget);
            if (reader.isStartElement() && reader.name() == "sst") {
                constexpr int kMaxReservedSharedStrings = 1'000'000;
                bool parsed = false;
                const auto count = reader.attributes().value("uniqueCount").toInt(&parsed);
                if (parsed && count > 0) {
                    values.reserve(
                        static_cast<std::size_t>((std::min)(count, kMaxReservedSharedStrings)));
                }
            }
            if (reader.isStartElement() && reader.name() == "si") {
                current.clear();
                inString = true;
                phoneticDepth = 0;
            } else if (inString && reader.isStartElement() && reader.name() == "rPh") {
                ++phoneticDepth;
            } else if (inString && reader.isEndElement() && reader.name() == "rPh" &&
                       phoneticDepth > 0) {
                --phoneticDepth;
            } else if (inString && phoneticDepth > 0) {
                continue;
            } else if (inString && reader.isStartElement() && reader.name() == "t") {
                readTextElementInto(reader, budget, textBudget, current);
            } else if (reader.isEndElement() && reader.name() == "si") {
                values.push_back(current.toStdString());
                inString = false;
            }
        }
        if (reader.hasError()) {
            throw std::runtime_error("cannot parse xlsx shared strings");
        }
        return values;
    }

    std::string
    XlsxWorkbookReader::relationshipIdForFirstWorkbookSheet(const std::string& xml,
                                                            const std::stop_token& stopToken) {
        QXmlStreamReader reader(xmlBytes(xml));
        while (!reader.atEnd()) {
            throwIfImportCanceled(stopToken);
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != "sheet") {
                continue;
            }
            const auto attributes = reader.attributes();
            const auto relationship = std::ranges::find_if(attributes, isRelationshipIdAttribute);
            if (relationship != attributes.end()) {
                return toStdString(relationship->value());
            }
        }
        throw std::runtime_error("xlsx workbook has no sheet relationship");
    }

    std::string
    XlsxWorkbookReader::worksheetEntryForRelationship(const WorksheetRelationshipRequest& request,
                                                      const std::stop_token& stopToken) {
        QXmlStreamReader reader(xmlBytes(request.xml));
        while (!reader.atEnd()) {
            throwIfImportCanceled(stopToken);
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != "Relationship") {
                continue;
            }
            std::string identifier;
            std::string target;
            for (const auto& attribute : reader.attributes()) {
                if (attribute.name() == "Id") {
                    identifier = toStdString(attribute.value());
                } else if (attribute.name() == "Target") {
                    target = toStdString(attribute.value());
                }
            }
            if (identifier == request.relationshipId && !target.empty()) {
                return normalizeWorksheetTarget(target);
            }
        }
        throw std::runtime_error("xlsx worksheet relationship not found");
    }

    std::vector<std::vector<std::string>>
    XlsxWorkbookReader::parseSheetRows(const std::string& xml,
                                       const std::vector<std::string>& sharedStrings,
                                       const std::vector<bool>& dateStyles, const bool date1904,
                                       const std::stop_token& stopToken) {
        return SheetRowsParser{xml, sharedStrings, dateStyles, date1904, stopToken}.parse();
    }

} // namespace ssa::infra::importing
