#include "presentation/AdvancedSectorHierarchyViewModel.h"

#include "domain/SectorHierarchy.h"

#include <QVariantMap>

namespace ssa::presentation {

    namespace {
        constexpr auto kExecutorColumn = "setor_executor";

        [[nodiscard]] QString includeExpression(const std::vector<std::string>& values) {
            QStringList tokens;
            tokens.reserve(static_cast<qsizetype>(values.size()));
            for (const auto& value : values) {
                tokens.push_back(QStringLiteral("=") + QString::fromStdString(value));
            }
            return tokens.join(QStringLiteral(","));
        }

        [[nodiscard]] QStringList includeTokens(const QString& expression) {
            QStringList result;
            for (const auto& part : expression.split(QStringLiteral(","), Qt::SkipEmptyParts)) {
                const auto token = part.trimmed();
                if (token.size() > 1 && token.startsWith(QStringLiteral("="))) {
                    result.push_back(token.sliced(1).trimmed().toUpper());
                }
            }
            result.removeDuplicates();
            return result;
        }
    } // namespace

    AdvancedSectorHierarchyViewModel::AdvancedSectorHierarchyViewModel(
        filterpanel::FilterPanelAdvancedState& state, QObject* parent)
        : QObject(parent), state_(state) {
        for (const auto& division : domain::SectorHierarchy::divisions()) {
            QStringList sectors;
            for (const auto sector : division.sectors) {
                sectors.push_back(
                    QString::fromUtf8(sector.data(), static_cast<qsizetype>(sector.size())));
            }
            divisions_.push_back(QVariantMap{
                {"key", QString::fromUtf8(division.key.data(),
                                          static_cast<qsizetype>(division.key.size()))},
                {"sectors", sectors},
            });
        }
    }

    const QVariantList& AdvancedSectorHierarchyViewModel::divisions() const {
        return divisions_;
    }

    QString AdvancedSectorHierarchyViewModel::selectedDivision() const {
        const auto selected = executorIncludeValues();
        if (selected.empty()) {
            return {};
        }
        for (const auto& division : domain::SectorHierarchy::divisions()) {
            const auto sectors = domain::SectorHierarchy::sectorsForDivision(division.key);
            if (selected.size() != static_cast<qsizetype>(sectors.size())) {
                continue;
            }
            bool allSelected = true;
            for (const auto& sector : sectors) {
                if (!selected.contains(QString::fromStdString(sector))) {
                    allSelected = false;
                    break;
                }
            }
            if (allSelected) {
                return QString::fromUtf8(division.key.data(),
                                         static_cast<qsizetype>(division.key.size()));
            }
        }
        return {};
    }

    void AdvancedSectorHierarchyViewModel::applyDivision(const QString& divisionKey) {
        const auto sectors =
            domain::SectorHierarchy::sectorsForDivision(divisionKey.trimmed().toStdString());
        if (sectors.empty()) {
            return;
        }
        if (!state_.setTextFilter(QString::fromLatin1(kExecutorColumn),
                                  includeExpression(sectors))) {
            return;
        }
        emit changed();
    }

    void AdvancedSectorHierarchyViewModel::clearDivision() {
        if (!state_.setTextFilter(QString::fromLatin1(kExecutorColumn), {})) {
            return;
        }
        emit changed();
    }

    void AdvancedSectorHierarchyViewModel::refreshFromState() {
        emit changed();
    }

    QStringList AdvancedSectorHierarchyViewModel::executorIncludeValues() const {
        return includeTokens(state_.textFilter(QString::fromLatin1(kExecutorColumn)));
    }

} // namespace ssa::presentation
