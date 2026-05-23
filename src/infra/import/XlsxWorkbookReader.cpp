#include "infra/import/XlsxWorkbookReader.h"

#include "infra/import/XlsxPackage.h"

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QXmlStreamReader>

#include <stdexcept>

namespace ssa::infra::importing {

    namespace {

        constexpr std::size_t kMaxWorksheetRows = 250'000;

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
            for (const auto ch : ref) {
                if (!ch.isLetter()) {
                    break;
                }
                hasLetters = true;
                index = index * 26 + (ch.toUpper().unicode() - QChar{'A'}.unicode() + 1);
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
            bool ok = false;
            const auto rows = lastCell.sliced(digitStart).toULongLong(&ok);
            return ok ? static_cast<std::size_t>(rows) : 0;
        }

        std::optional<std::size_t> rowIndexFromRowRef(const QStringView ref) {
            if (ref.empty()) {
                return std::nullopt;
            }
            bool ok = false;
            const auto row = ref.toULongLong(&ok);
            if (!ok || row == 0 || row > kMaxWorksheetRows) {
                throw std::runtime_error("xlsx row index out of supported range");
            }
            return static_cast<std::size_t>(row - 1);
        }

        class XmlReadBudget final {
          public:
            void record(const QXmlStreamReader& reader) {
                constexpr std::size_t kMaxXmlTokens = 20'000'000;
                constexpr int kMaxXmlDepth = 64;
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
            std::size_t tokenCount_{0};
            int depth_{0};
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
            qsizetype totalCharacters_{0};
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

        std::string decodeCellValue(const QString& rawValue, const QStringView type,
                                    const std::vector<std::string>& sharedStrings) {
            if (type == "s") {
                bool ok = false;
                const auto index = rawValue.toInt(&ok);
                if (ok && index >= 0 && static_cast<std::size_t>(index) < sharedStrings.size()) {
                    return sharedStrings[static_cast<std::size_t>(index)];
                }
                return {};
            }
            return rawValue.toStdString();
        }

        QString readCellRawValue(QXmlStreamReader& reader, XmlReadBudget& budget,
                                 XmlTextBudget& textBudget) {
            QString rawValue;
            while (!reader.atEnd() && !reader.hasError()) {
                reader.readNext();
                if (reader.atEnd()) {
                    break;
                }
                recordXmlToken(reader, budget);
                if (reader.isStartElement() && (reader.name() == "v" || reader.name() == "t")) {
                    while (!reader.atEnd() && !reader.hasError()) {
                        reader.readNext();
                        if (reader.atEnd()) {
                            break;
                        }
                        recordXmlToken(reader, budget);
                        if (reader.isCharacters()) {
                            appendCharacters(rawValue, reader.text(), textBudget);
                        } else if (reader.isEndElement() &&
                                   (reader.name() == "v" || reader.name() == "t")) {
                            break;
                        }
                    }
                } else if (reader.isEndElement() && reader.name() == "c") {
                    break;
                }
            }
            return rawValue;
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

    } // namespace

    SpreadsheetTable
    XlsxWorkbookReader::readFirstSheet(const std::filesystem::path& filePath) const {
        XlsxPackage package(filePath);
        const auto workbook = package.textEntry("xl/workbook.xml", true);
        const auto relationshipId = relationshipIdForFirstWorkbookSheet(workbook);
        const auto relationships = package.textEntry("xl/_rels/workbook.xml.rels", true);
        const auto worksheetEntry = worksheetEntryForRelationship(relationships, relationshipId);
        const auto sharedStrings =
            parseSharedStrings(package.textEntry("xl/sharedStrings.xml", false));
        const auto sheetXml = package.textEntry(worksheetEntry, true);
        return SpreadsheetTable{filePath, parseSheetRows(sheetXml, sharedStrings)};
    }

    std::vector<std::string> XlsxWorkbookReader::parseSharedStrings(const std::string& xml) {
        std::vector<std::string> values;
        if (xml.empty()) {
            return values;
        }
        QXmlStreamReader reader(xmlBytes(xml));
        QString current;
        current.reserve(256);
        bool inString = false;
        int phoneticDepth = 0;
        XmlReadBudget budget;
        XmlTextBudget textBudget;
        while (!reader.atEnd()) {
            reader.readNext();
            recordXmlToken(reader, budget);
            if (reader.isStartElement() && reader.name() == "sst") {
                constexpr int kMaxReservedSharedStrings = 1'000'000;
                bool ok = false;
                const auto count = reader.attributes().value("uniqueCount").toInt(&ok);
                if (ok && count > 0) {
                    values.reserve(
                        static_cast<std::size_t>(std::min(count, kMaxReservedSharedStrings)));
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

    std::string XlsxWorkbookReader::relationshipIdForFirstWorkbookSheet(const std::string& xml) {
        QXmlStreamReader reader(xmlBytes(xml));
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != "sheet") {
                continue;
            }
            for (const auto& attribute : reader.attributes()) {
                if (isRelationshipIdAttribute(attribute)) {
                    return toStdString(attribute.value());
                }
            }
        }
        throw std::runtime_error("xlsx workbook has no sheet relationship");
    }

    std::string
    XlsxWorkbookReader::worksheetEntryForRelationship(const std::string& xml,
                                                      const std::string& relationshipId) {
        QXmlStreamReader reader(xmlBytes(xml));
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != "Relationship") {
                continue;
            }
            std::string id;
            std::string target;
            for (const auto& attribute : reader.attributes()) {
                if (attribute.name() == "Id") {
                    id = toStdString(attribute.value());
                } else if (attribute.name() == "Target") {
                    target = toStdString(attribute.value());
                }
            }
            if (id == relationshipId && !target.empty()) {
                return normalizeWorksheetTarget(target);
            }
        }
        throw std::runtime_error("xlsx worksheet relationship not found");
    }

    std::vector<std::vector<std::string>>
    XlsxWorkbookReader::parseSheetRows(const std::string& xml,
                                       const std::vector<std::string>& sharedStrings) {
        std::vector<std::vector<std::string>> rows;
        QXmlStreamReader reader(xmlBytes(xml));
        std::vector<std::string> currentRow;
        bool inRow = false;
        std::optional<std::size_t> currentRowIndex;
        std::size_t nextColumnIndex = 0;
        std::size_t expectedColumns = 0;
        std::size_t totalCells = 0;
        XmlReadBudget budget;
        XmlTextBudget textBudget;
        while (!reader.atEnd()) {
            reader.readNext();
            recordXmlToken(reader, budget);
            if (reader.isStartElement() && reader.name() == "dimension") {
                expectedColumns =
                    absoluteColumnSlotsFromDimensionRef(reader.attributes().value("ref"));
                if (expectedColumns > 0) {
                    currentRow.reserve(expectedColumns);
                }
                const auto expectedRows =
                    absoluteRowSlotsFromDimensionRef(reader.attributes().value("ref"));
                if (expectedRows > 0) {
                    rows.reserve(std::min(expectedRows, kMaxWorksheetRows));
                }
                continue;
            }
            if (reader.isStartElement() && reader.name() == "row") {
                currentRow.clear();
                if (expectedColumns > 0) {
                    currentRow.reserve(expectedColumns);
                }
                currentRowIndex = rowIndexFromRowRef(reader.attributes().value("r"));
                nextColumnIndex = 0;
                inRow = true;
                continue;
            }
            if (inRow && reader.isStartElement() && reader.name() == "c") {
                const auto attributes = reader.attributes();
                const auto ref = attributes.value("r");
                const auto type = attributes.value("t");
                const auto columnIndex = columnIndexFromCellRef(ref);
                const QString rawValue = readCellRawValue(reader, budget, textBudget);
                if (columnIndex || ref.isEmpty()) {
                    const auto resolvedColumn =
                        columnIndex ? static_cast<std::size_t>(*columnIndex) : nextColumnIndex;
                    currentRow.resize(std::max<std::size_t>(
                        currentRow.size(), static_cast<std::size_t>(resolvedColumn + 1)));
                    currentRow[resolvedColumn] = decodeCellValue(rawValue, type, sharedStrings);
                    nextColumnIndex = resolvedColumn + 1;
                    ++totalCells;
                    constexpr std::size_t kMaxWorksheetCells = 5'000'000;
                    if (totalCells > kMaxWorksheetCells) {
                        throw std::runtime_error("xlsx worksheet has too many cells");
                    }
                }
                continue;
            }
            if (reader.isEndElement() && reader.name() == "row") {
                if (currentRowIndex) {
                    rows.resize(std::max(rows.size(), *currentRowIndex + 1));
                    rows[*currentRowIndex] = std::move(currentRow);
                } else {
                    rows.push_back(std::move(currentRow));
                }
                if (rows.size() > kMaxWorksheetRows) {
                    throw std::runtime_error("xlsx worksheet has too many rows");
                }
                inRow = false;
                currentRowIndex.reset();
            }
        }
        if (reader.hasError()) {
            throw std::runtime_error("cannot parse xlsx worksheet");
        }
        return rows;
    }

} // namespace ssa::infra::importing
