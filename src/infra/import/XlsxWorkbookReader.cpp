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

        constexpr std::size_t kMaxWorksheetRows = 250'015;
        constexpr std::size_t kMaxWorksheetCells = 5'000'000;
        constexpr std::size_t kMaxWorksheets = 256;
        constexpr std::size_t kMaxSharedStrings = 1'000'000;
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
            SheetRowsParser(const std::vector<std::string>& sharedStrings,
                            const std::vector<bool>& dateStyles, const bool date1904,
                            const std::size_t rowsPerChunk,
                            const XlsxWorkbookReader::SheetChunkConsumer& consume,
                            SpreadsheetTable metadata, std::stop_token stopToken)
                : sharedStrings_(sharedStrings), dateStyles_(dateStyles), date1904_(date1904),
                  rowsPerChunk_(rowsPerChunk), consume_(consume), metadata_(std::move(metadata)),
                  readBudget_(std::move(stopToken)) {
                if (rowsPerChunk_ == 0 || rowsPerChunk_ > kMaxWorksheetRows) {
                    throw std::invalid_argument("xlsx chunk row count is out of range");
                }
                pendingRows_.reserve(rowsPerChunk_);
            }

            void addData(const std::string_view data) {
                reader_.addData(QByteArray{data.data(), static_cast<qsizetype>(data.size())});
                parseAvailable();
            }

            void finish() {
                parseAvailable();
                if (!documentComplete_ || reader_.hasError()) {
                    throw std::runtime_error("cannot parse xlsx worksheet");
                }
                flush(true);
            }

          private:
            void parseAvailable() {
                while (!reader_.atEnd()) {
                    const auto token = reader_.readNext();
                    if (token == QXmlStreamReader::Invalid &&
                        reader_.error() == QXmlStreamReader::PrematureEndOfDocumentError) {
                        return;
                    }
                    recordXmlToken(reader_, readBudget_);
                    if (reader_.isStartElement() && reader_.name() == "dimension") {
                        handleDimension();
                    } else if (reader_.isStartElement() && reader_.name() == "row") {
                        startRow();
                    } else if (inRow_ && reader_.isStartElement() && reader_.name() == "c") {
                        startCell();
                    } else if (inCell_) {
                        handleCellToken();
                    } else if (reader_.isEndElement() && reader_.name() == "row") {
                        finishRow();
                    } else if (reader_.isEndDocument()) {
                        documentComplete_ = true;
                    }
                }
                if (reader_.hasError() &&
                    reader_.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
                    throw std::runtime_error("cannot parse xlsx worksheet");
                }
            }

            void handleDimension() {
                expectedColumns_ =
                    absoluteColumnSlotsFromDimensionRef(reader_.attributes().value("ref"));
                if (expectedColumns_ > 0) {
                    currentRow_.reserve(expectedColumns_);
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

            void startCell() {
                const auto attributes = reader_.attributes();
                cellRef_ = attributes.value("r").toString();
                cellType_ = attributes.value("t").toString();
                cellStyleIndex_ = attributes.value("s").toUInt(&cellStyleParsed_);
                cellValue_ = {};
                inCell_ = true;
                inCellValue_ = false;
            }

            void handleCellToken() {
                if (reader_.isStartElement() && reader_.name() == "f") {
                    cellValue_.hasFormula = true;
                } else if (reader_.isStartElement() &&
                           (reader_.name() == "v" || reader_.name() == "t")) {
                    cellValue_.hasCachedValue = true;
                    inCellValue_ = true;
                } else if (inCellValue_ && reader_.isCharacters()) {
                    appendCharacters(cellValue_.text, reader_.text(), textBudget_);
                } else if (reader_.isEndElement() &&
                           (reader_.name() == "v" || reader_.name() == "t")) {
                    inCellValue_ = false;
                } else if (reader_.isEndElement() && reader_.name() == "c") {
                    finishCell();
                }
            }

            void finishCell() {
                if (cellValue_.hasFormula && !cellValue_.hasCachedValue) {
                    throw std::runtime_error("xlsx formula cell has no cached value");
                }
                const auto columnIndex = columnIndexFromCellRef(cellRef_);
                if (!columnIndex && !cellRef_.isEmpty()) {
                    inCell_ = false;
                    return;
                }
                const auto resolvedColumn =
                    columnIndex ? static_cast<std::size_t>(*columnIndex) : nextColumnIndex_;
                currentRow_.resize(std::max<std::size_t>(currentRow_.size(), resolvedColumn + 1));
                const bool dateStyle = cellStyleParsed_ && cellStyleIndex_ < dateStyles_.size() &&
                                       dateStyles_[cellStyleIndex_];
                currentRow_[resolvedColumn] = decodeCellValue(cellValue_.text, cellType_,
                                                              sharedStrings_, dateStyle, date1904_);
                nextColumnIndex_ = resolvedColumn + 1;
                ++totalCells_;
                if (totalCells_ > kMaxWorksheetCells) {
                    throw std::runtime_error("xlsx worksheet has too many cells");
                }
                inCell_ = false;
            }

            void finishRow() {
                const auto rowIndex = currentRowIndex_.value_or(nextRowIndex_);
                if (rowIndex < nextRowIndex_) {
                    throw std::runtime_error("xlsx rows are not ordered");
                }
                while (nextRowIndex_ < rowIndex) {
                    appendRow({});
                }
                appendRow(std::move(currentRow_));
                currentRow_ = {};
                inRow_ = false;
                currentRowIndex_.reset();
            }

            void appendRow(std::vector<std::string> row) {
                if (pendingRows_.size() == rowsPerChunk_) {
                    flush(false);
                }
                pendingRows_.push_back(std::move(row));
                ++nextRowIndex_;
                if (nextRowIndex_ > kMaxWorksheetRows) {
                    throw std::runtime_error("xlsx worksheet has too many rows");
                }
            }

            void flush(const bool lastInSheet) {
                if (pendingRows_.empty() && emittedChunk_) {
                    return;
                }
                auto chunk = metadata_;
                chunk.rows.swap(pendingRows_);
                pendingRows_.reserve(rowsPerChunk_);
                consume_(std::move(chunk), !emittedChunk_, lastInSheet);
                emittedChunk_ = true;
            }

            QXmlStreamReader reader_;
            const std::vector<std::string>& sharedStrings_;
            const std::vector<bool>& dateStyles_;
            bool date1904_ = false;
            std::size_t rowsPerChunk_;
            const XlsxWorkbookReader::SheetChunkConsumer& consume_;
            SpreadsheetTable metadata_;
            std::vector<std::vector<std::string>> pendingRows_;
            std::vector<std::string> currentRow_;
            QString cellRef_;
            QString cellType_;
            CellRawValue cellValue_;
            bool inRow_ = false;
            bool inCell_ = false;
            bool inCellValue_ = false;
            bool cellStyleParsed_ = false;
            std::optional<std::size_t> currentRowIndex_ = std::nullopt;
            std::size_t nextColumnIndex_ = 0;
            std::size_t expectedColumns_ = 0;
            std::size_t totalCells_ = 0;
            std::size_t nextRowIndex_ = 0;
            unsigned int cellStyleIndex_ = 0;
            bool emittedChunk_ = false;
            bool documentComplete_ = false;
            XmlReadBudget readBudget_;
            XmlTextBudget textBudget_;
        };

    } // namespace

    SpreadsheetTable XlsxWorkbookReader::readFirstSheet(const std::filesystem::path& filePath,
                                                        const std::stop_token& stopToken) {
        throwIfImportCanceled(stopToken);
        XlsxPackage package(filePath);
        const auto workbook = package.textEntry("xl/workbook.xml", true, stopToken);
        const auto relationshipIds = relationshipIdsForWorkbookSheets(workbook, stopToken);
        throwIfImportCanceled(stopToken);
        const auto relationships = package.textEntry("xl/_rels/workbook.xml.rels", true, stopToken);
        const auto worksheetEntry =
            worksheetEntryForRelationship({relationships, relationshipIds.front()}, stopToken);
        const auto sharedStrings = parseSharedStrings(
            package.textEntry("xl/sharedStrings.xml", false, stopToken), stopToken);
        const auto dateStyles =
            parseDateStyles(package.textEntry("xl/styles.xml", false, stopToken), stopToken);
        const bool date1904 = usesDate1904(workbook, stopToken);
        throwIfImportCanceled(stopToken);
        const auto sheetXml = package.textEntry(worksheetEntry, true, stopToken);
        SpreadsheetTable table;
        table.sourcePath = filePath;
        parseSheetRowChunks(
            sheetXml, sharedStrings, dateStyles, date1904, kMaxWorksheetRows,
            [&](SpreadsheetTable chunk, const bool, const bool) { table = std::move(chunk); },
            table, stopToken);
        return table;
    }

    std::vector<SpreadsheetTable>
    XlsxWorkbookReader::readSheets(const std::filesystem::path& filePath,
                                   const std::stop_token& stopToken) {
        throwIfImportCanceled(stopToken);
        XlsxPackage package(filePath);
        const auto workbook = package.textEntry("xl/workbook.xml", true, stopToken);
        const auto relationshipIds = relationshipIdsForWorkbookSheets(workbook, stopToken);
        const auto relationships = package.textEntry("xl/_rels/workbook.xml.rels", true, stopToken);
        const auto sharedStrings = parseSharedStrings(
            package.textEntry("xl/sharedStrings.xml", false, stopToken), stopToken);
        const auto dateStyles =
            parseDateStyles(package.textEntry("xl/styles.xml", false, stopToken), stopToken);
        const bool date1904 = usesDate1904(workbook, stopToken);

        std::vector<SpreadsheetTable> tables;
        tables.reserve(relationshipIds.size());
        for (const auto& relationshipId : relationshipIds) {
            throwIfImportCanceled(stopToken);
            const auto worksheetEntry =
                worksheetEntryForRelationship({relationships, relationshipId}, stopToken);
            const auto sheetXml = package.textEntry(worksheetEntry, true, stopToken);
            SpreadsheetTable table;
            table.sourcePath = filePath;
            parseSheetRowChunks(
                sheetXml, sharedStrings, dateStyles, date1904, kMaxWorksheetRows,
                [&](SpreadsheetTable chunk, const bool, const bool) { table = std::move(chunk); },
                table, stopToken);
            tables.push_back(std::move(table));
        }
        return tables;
    }

    void XlsxWorkbookReader::readSheetChunks(const std::filesystem::path& filePath,
                                             const std::size_t rowsPerChunk,
                                             const SheetChunkConsumer& consume,
                                             const std::stop_token& stopToken) {
        throwIfImportCanceled(stopToken);
        XlsxPackage package(filePath);
        const auto workbook = package.textEntry("xl/workbook.xml", true, stopToken);
        const auto relationshipIds = relationshipIdsForWorkbookSheets(workbook, stopToken);
        const auto relationships = package.textEntry("xl/_rels/workbook.xml.rels", true, stopToken);
        const auto sharedStrings = parseSharedStrings(
            package.textEntry("xl/sharedStrings.xml", false, stopToken), stopToken);
        const auto dateStyles =
            parseDateStyles(package.textEntry("xl/styles.xml", false, stopToken), stopToken);
        const bool date1904 = usesDate1904(workbook, stopToken);
        for (const auto& relationshipId : relationshipIds) {
            throwIfImportCanceled(stopToken);
            const auto worksheetEntry =
                worksheetEntryForRelationship({relationships, relationshipId}, stopToken);
            SpreadsheetTable metadata;
            metadata.sourcePath = filePath;
            SheetRowsParser parser{sharedStrings, dateStyles, date1904, rowsPerChunk,
                                   consume,       metadata,   stopToken};
            package.streamTextEntry(
                worksheetEntry, true, [&](const std::string_view chunk) { parser.addData(chunk); },
                stopToken);
            parser.finish();
        }
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
                bool parsed = false;
                const auto count = reader.attributes().value("uniqueCount").toInt(&parsed);
                if (parsed && count > static_cast<int>(kMaxSharedStrings)) {
                    throw std::runtime_error("xlsx shared strings exceed supported limit");
                }
                if (parsed && count > 0) {
                    values.reserve(static_cast<std::size_t>(count));
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
                if (values.size() >= kMaxSharedStrings) {
                    throw std::runtime_error("xlsx shared strings exceed supported limit");
                }
                values.push_back(current.toStdString());
                inString = false;
            }
        }
        if (reader.hasError()) {
            throw std::runtime_error("cannot parse xlsx shared strings");
        }
        return values;
    }

    std::vector<std::string>
    XlsxWorkbookReader::relationshipIdsForWorkbookSheets(const std::string& xml,
                                                         const std::stop_token& stopToken) {
        std::vector<std::string> relationships;
        std::unordered_set<std::string> seenRelationships;
        QXmlStreamReader reader(xmlBytes(xml));
        while (!reader.atEnd()) {
            throwIfImportCanceled(stopToken);
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != "sheet") {
                continue;
            }
            const auto attributes = reader.attributes();
            const auto relationship = std::ranges::find_if(attributes, isRelationshipIdAttribute);
            if (relationship == attributes.end()) {
                throw std::runtime_error("xlsx worksheet has no relationship id");
            }
            auto identifier = toStdString(relationship->value());
            if (identifier.empty() || !seenRelationships.insert(identifier).second) {
                throw std::runtime_error("xlsx worksheet relationship id is invalid or duplicated");
            }
            relationships.push_back(std::move(identifier));
            if (relationships.size() > kMaxWorksheets) {
                throw std::runtime_error("xlsx workbook has too many worksheets");
            }
        }
        if (reader.hasError()) {
            throw std::runtime_error("cannot parse xlsx workbook sheets");
        }
        if (relationships.empty()) {
            throw std::runtime_error("xlsx workbook has no sheet relationship");
        }
        return relationships;
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

    void XlsxWorkbookReader::parseSheetRowChunks(
        const std::string& xml, const std::vector<std::string>& sharedStrings,
        const std::vector<bool>& dateStyles, const bool date1904, const std::size_t rowsPerChunk,
        const SheetChunkConsumer& consume, const SpreadsheetTable& metadata,
        const std::stop_token& stopToken) {
        SheetRowsParser parser{sharedStrings, dateStyles, date1904, rowsPerChunk,
                               consume,       metadata,   stopToken};
        parser.addData(xml);
        parser.finish();
    }

} // namespace ssa::infra::importing
