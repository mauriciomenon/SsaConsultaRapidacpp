#include "infra/import/XlsxWorkbookReader.h"

#include "infra/import/XlsxPackage.h"

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QXmlStreamReader>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
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

        std::string decodeCellValue(const QString& rawValue, const QStringView type,
                                    const std::vector<std::string>& sharedStrings) {
            if (type == "s") {
                bool parsed = false;
                const auto index = rawValue.toInt(&parsed);
                if (parsed && index >= 0 &&
                    static_cast<std::size_t>(index) < sharedStrings.size()) {
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

        class SheetRowsParser final {
          public:
            SheetRowsParser(const std::string& xml, const std::vector<std::string>& sharedStrings,
                            std::stop_token stopToken)
                : reader_(xmlBytes(xml)), sharedStrings_(sharedStrings),
                  readBudget_(std::move(stopToken)) {}

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
                const auto columnIndex = columnIndexFromCellRef(ref);
                const QString rawValue = readCellRawValue(reader_, readBudget_, textBudget_);
                if (!columnIndex && !ref.isEmpty()) {
                    return;
                }
                const auto resolvedColumn =
                    columnIndex ? static_cast<std::size_t>(*columnIndex) : nextColumnIndex_;
                currentRow_.resize(std::max<std::size_t>(currentRow_.size(), resolvedColumn + 1));
                currentRow_[resolvedColumn] = decodeCellValue(rawValue, type, sharedStrings_);
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
                                                        const std::stop_token stopToken) {
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
        throwIfImportCanceled(stopToken);
        const auto sheetXml = package.textEntry(worksheetEntry, true, stopToken);
        return SpreadsheetTable{filePath, parseSheetRows(sheetXml, sharedStrings, stopToken)};
    }

    std::vector<std::string>
    XlsxWorkbookReader::parseSharedStrings(const std::string& xml,
                                           const std::stop_token stopToken) {
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
                                                            const std::stop_token stopToken) {
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
                                                      const std::stop_token stopToken) {
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
                                       const std::stop_token stopToken) {
        return SheetRowsParser{xml, sharedStrings, stopToken}.parse();
    }

} // namespace ssa::infra::importing
