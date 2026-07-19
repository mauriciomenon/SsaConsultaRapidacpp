#include "domain/SsaTypes.h"
#include "ports/ISsaRepository.h"
#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/DerivadasGraphModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelDistinctValueRequestBuilder.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableModel.h"
#include "query/SsaQueryService.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJSValue>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqml.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {

    class CountingRepository final : public ssa::ports::ISsaRepository {
      public:
        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token = {}) const override {
            return {{}, 0, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token = {}) const override {
            return 0;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return std::nullopt;
        }

        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return {};
        }

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest& request,
                                                std::stop_token = {}) const override {
            if (request.limit == ssa::presentation::kAdvancedDistinctValuesLimit) {
                advancedRequests_.fetch_add(1, std::memory_order_relaxed);
                if (failNextAdvancedRequest_.exchange(false, std::memory_order_acq_rel)) {
                    throw std::runtime_error("advanced values failed once");
                }
            }
            if (request.columnKey == "num_reprogramacoes") {
                reprogrammingRequests_.fetch_add(1, std::memory_order_relaxed);
                return {"0", "1", "3"};
            }
            if (request.columnKey == "setor_executor") {
                quickSectorRequests_.fetch_add(1, std::memory_order_relaxed);
                if (failNextQuickSectorRequest_.exchange(false, std::memory_order_acq_rel)) {
                    throw std::runtime_error("quick sector failed once");
                }
                return {"MEG2"};
            }
            return {"APV"};
        }

        std::size_t maxValueLength(std::string_view, std::stop_token = {}) const override {
            return 3;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] int advancedRequests() const {
            return advancedRequests_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] int reprogrammingRequests() const {
            return reprogrammingRequests_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] int quickSectorRequests() const {
            return quickSectorRequests_.load(std::memory_order_relaxed);
        }

        void failNextAdvancedRequest() {
            failNextAdvancedRequest_.store(true, std::memory_order_release);
        }

        void failNextQuickSectorRequest() {
            failNextQuickSectorRequest_.store(true, std::memory_order_release);
        }

      private:
        mutable std::atomic<int> advancedRequests_{0};
        mutable std::atomic<int> reprogrammingRequests_{0};
        mutable std::atomic<int> quickSectorRequests_{0};
        mutable std::atomic_bool failNextAdvancedRequest_{false};
        mutable std::atomic_bool failNextQuickSectorRequest_{false};
    };

    [[nodiscard]] QDir repositoryRoot() {
        QDir root = QFileInfo(QString::fromUtf8(__FILE__)).dir();
        if (!root.cdUp() || !root.cdUp()) {
            qFatal("test repository root could not be resolved");
        }
        return root;
    }

    [[nodiscard]] QQuickItem* findQuickItemByProperty(QQuickItem* parent, const char* propertyName,
                                                      const QString& expectedValue) {
        for (auto* child : parent->childItems()) {
            if (child->property(propertyName).toString() == expectedValue) {
                return child;
            }
            if (auto* match = findQuickItemByProperty(child, propertyName, expectedValue)) {
                return match;
            }
        }
        return nullptr;
    }

    [[nodiscard]] QQuickItem* findOwnedQuickItemByProperty(QObject* parent,
                                                           const char* propertyName,
                                                           const QString& expectedValue) {
        for (auto* object : parent->findChildren<QObject*>()) {
            auto* item = qobject_cast<QQuickItem*>(object);
            if (item != nullptr && item->property(propertyName).toString() == expectedValue) {
                return item;
            }
        }
        return nullptr;
    }

    [[nodiscard]] QPoint clickPointInWindow(const QQuickItem& item) {
        return item.mapToScene(item.boundingRect().center()).toPoint();
    }

    [[nodiscard]] int countQuickItemsByObjectName(QQuickItem* parent, const QString& objectName) {
        int count = 0;
        for (auto* child : parent->childItems()) {
            if (child->objectName() == objectName) {
                ++count;
            }
            count += countQuickItemsByObjectName(child, objectName);
        }
        return count;
    }

    [[nodiscard]] qreal linearChannel(const qreal channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    }

    [[nodiscard]] qreal contrastRatio(const QColor& first, const QColor& second) {
        const auto luminance = [](const QColor& color) {
            return 0.2126 * linearChannel(color.redF()) + 0.7152 * linearChannel(color.greenF()) +
                   0.0722 * linearChannel(color.blueF());
        };
        const qreal lighter = std::max(luminance(first), luminance(second));
        const qreal darker = std::min(luminance(first), luminance(second));
        return (lighter + 0.05) / (darker + 0.05);
    }

    constexpr auto kReprogrammingHarness = R"QML(
        import QtQuick
        import QtQuick.Controls
        import SsaConsultaRapida

        Item {
            id: harness
            width: 600
            height: 500
            property int applyCount: 0

            AdvancedReprogrammingFilterCard {
                x: 100
                y: 100
                filterViewModel: testFilterViewModel
                derivation: testFilterViewModel.advanced.derivation
                cardWidth: 360
                cardHeight: 52
                onApplyRequested: harness.applyCount += 1
            }
        }
    )QML";

    constexpr auto kMacroReportHarness = R"QML(
        import QtQuick
        import QtQuick.Controls
        import SsaConsultaRapida

        Item {
            id: harness
            width: 420
            height: 160

            QtObject {
                id: macroModel
                property var options: []
                property string selectedMacro: ""
                property string reportTitle: "SSA Executadas Setor"
                property string reportText: "5000 linhas"
                property var reportRows: []
                property bool reportLoading: false
                property string reportError: ""
            }

            AdvancedMacroFilterCard {
                objectName: "macroReportCard"
                anchors.fill: parent
                macro: macroModel
                cardWidth: width
                cardHeight: height
            }

            Component.onCompleted: {
                const rows = [];
                for (let index = 0; index < 5000; ++index) {
                    rows.push({
                        group: "MEG",
                        week: "202601",
                        person: "Pessoa " + index,
                        count: 1
                    });
                }
                macroModel.reportRows = rows;
            }
        }
    )QML";

    constexpr auto kAdvancedTextHarness = R"QML(
        import QtQuick
        import QtQuick.Controls
        import SsaConsultaRapida

        Item {
            width: 600
            height: 500

            AdvancedTextFilterCard {
                id: card
                x: 100
                y: 100
                property var loadedValues: []
                property bool loadedValuesLoading: false
                property int loadedMaxValueLength: 0
                property string loadedValuesError: ""

                key: "situacao"
                label: "Situacao"
                operatorModes: [{ label: "=", mode: "equals" }]
                allValues: loadedValues
                visibleValues: loadedValues
                valuesLoading: loadedValuesLoading
                valuesError: loadedValuesError
                maxValueLength: loadedMaxValueLength
                textFilter: ""
                operatorIndex: 0
                operatorLabel: "="
                cardWidth: 360
                cardHeight: 52

                function reloadOptionState() {
                    loadedValues = filterViewModel.columnValueOptionsFor(key);
                    loadedValuesLoading = filterViewModel.columnValueOptionsLoadingFor(key);
                    loadedMaxValueLength = filterViewModel.columnValueMaxLengthFor(key);
                    loadedValuesError = filterViewModel.columnValueOptionsErrorFor(key);
                }

                Connections {
                    target: filterViewModel
                    function onColumnValueOptionsChangedFor(key) {
                        if (key === card.key)
                            card.reloadOptionState();
                    }
                }

                onOptionsRequested: {
                    if (loadedValues.length === 0 && !loadedValuesLoading)
                        filterViewModel.refreshColumnValueOptionsFor(key);
                }
            }
        }
    )QML";

    constexpr auto kQuickSectorHarness = R"QML(
        import QtQuick
        import SsaConsultaRapida

        Item {
            width: 1000
            height: 60

            QtObject {
                id: browseModel
                property int pageNumber: 1
                property int pageCount: 1
                property int pageSize: 50
                function previousPage() {}
                function nextPage() {}
                function apply() {}
            }

            PagerQuickFilters {
                anchors.fill: parent
                viewModel: browseModel
                filterViewModel: testFilterViewModel
            }
        }
    )QML";

    class AdvancedPopupQmlTest final : public QObject {
        Q_OBJECT

      private slots:
        void initTestCase() {
            const QDir root = repositoryRoot();
            const QUrl themeUrl = QUrl::fromLocalFile(root.filePath("app/desktop/qml/Theme.qml"));
            QVERIFY(qmlRegisterSingletonType(themeUrl, "SsaConsultaRapida", 1, 0, "Theme") >= 0);
            const QDir components(root.filePath("app/desktop/qml/components"));
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("ActionButton.qml")),
                                    "SsaConsultaRapida", 1, 0, "ActionButton") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("SummaryTag.qml")),
                                    "SsaConsultaRapida", 1, 0, "SummaryTag") >= 0);
            QVERIFY(
                qmlRegisterType(QUrl::fromLocalFile(components.filePath("FilterSummaryBar.qml")),
                                "SsaConsultaRapida", 1, 0, "FilterSummaryBar") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppTextField.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppTextField") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppCheckBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppCheckBox") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppComboBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppComboBox") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppSpinBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppSpinBox") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("FilterCard.qml")),
                                    "SsaConsultaRapida", 1, 0, "FilterCard") >= 0);
            QVERIFY(qmlRegisterType(
                        QUrl::fromLocalFile(components.filePath("AdvancedTextValuePopup.qml")),
                        "SsaConsultaRapida", 1, 0, "AdvancedTextValuePopup") >= 0);
            QVERIFY(qmlRegisterType(
                        QUrl::fromLocalFile(components.filePath("AdvancedTextFilterCard.qml")),
                        "SsaConsultaRapida", 1, 0, "AdvancedTextFilterCard") >= 0);
            QVERIFY(qmlRegisterType(
                        QUrl::fromLocalFile(components.filePath("ReprogrammingValuePopup.qml")),
                        "SsaConsultaRapida", 1, 0, "ReprogrammingValuePopup") >= 0);
            QVERIFY(
                qmlRegisterType(
                    QUrl::fromLocalFile(components.filePath("AdvancedReprogrammingFilterCard.qml")),
                    "SsaConsultaRapida", 1, 0, "AdvancedReprogrammingFilterCard") >= 0);
            QVERIFY(qmlRegisterType(
                        QUrl::fromLocalFile(components.filePath("AdvancedMacroFilterCard.qml")),
                        "SsaConsultaRapida", 1, 0, "AdvancedMacroFilterCard") >= 0);
            QVERIFY(qmlRegisterType(
                        QUrl::fromLocalFile(components.filePath("DetailsRelationNavigator.qml")),
                        "SsaConsultaRapida", 1, 0, "DetailsRelationNavigator") >= 0);
            QVERIFY(
                qmlRegisterType(QUrl::fromLocalFile(components.filePath("SavedFilterControls.qml")),
                                "SsaConsultaRapida", 1, 0, "SavedFilterControls") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("DerivadasGraph.qml")),
                                    "SsaConsultaRapida", 1, 0, "DerivadasGraph") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("SsaTable.qml")),
                                    "SsaConsultaRapida", 1, 0, "SsaTable") >= 0);
            QVERIFY(
                qmlRegisterType(QUrl::fromLocalFile(components.filePath("PagerQuickFilters.qml")),
                                "SsaConsultaRapida", 1, 0, "PagerQuickFilters") >= 0);
        }

        void reprogramming_value_combo_loads_cold_cache_once() {
            auto repository = std::make_shared<CountingRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("testFilterViewModel"),
                                                     &filters);
            QQmlComponent component(&engine);
            component.setData(kReprogrammingHarness,
                              QUrl(QStringLiteral("inmemory:/ReprogrammingValueHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 600, 500);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* valueSelector =
                findQuickItemByProperty(harnessItem, "displayText", QStringLiteral("Valor"));
            QVERIFY(valueSelector != nullptr);
            QVERIFY(valueSelector->isEnabled());
            QCOMPARE(repository->reprogrammingRequests(), 0);

            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*valueSelector));

            QTRY_COMPARE_WITH_TIMEOUT(repository->reprogrammingRequests(), 1, 1000);
            const QStringList expectedValues{QStringLiteral("0"), QStringLiteral("1"),
                                             QStringLiteral("3")};
            QTRY_COMPARE_WITH_TIMEOUT(
                filters.columnValueOptionsFor(QStringLiteral("num_reprogramacoes")), expectedValues,
                1000);
            QTRY_COMPARE_WITH_TIMEOUT(valueSelector->property("count").toInt(), 4, 1000);
            QTest::keyClick(&window, Qt::Key_Escape);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*valueSelector));
            QCOMPARE(filters.columnValueOptionsLoadingFor(QStringLiteral("num_reprogramacoes")),
                     false);
            QCoreApplication::processEvents();
            QCOMPARE(repository->reprogrammingRequests(), 1);
        }

        void reprogramming_only_checkbox_commits_only_when_apply_is_clicked() {
            auto repository = std::make_shared<CountingRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(derivation != nullptr);
            derivation->setReprogrammingValues({QStringLiteral("1")});
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("testFilterViewModel"),
                                                     &filters);
            QQmlComponent component(&engine);
            component.setData(kReprogrammingHarness,
                              QUrl(QStringLiteral("inmemory:/ReprogrammingPopupHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 600, 500);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* openButton = findQuickItemByProperty(harnessItem, "text", QStringLiteral("..."));
            QVERIFY(openButton != nullptr);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*openButton));
            auto* onlyReprogrammed = findQuickItemByProperty(window.contentItem(), "text",
                                                             QStringLiteral("Todas com Reprog."));
            QTRY_VERIFY_WITH_TIMEOUT(onlyReprogrammed != nullptr && onlyReprogrammed->isVisible(),
                                     1000);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*onlyReprogrammed));
            auto* card = findOwnedQuickItemByProperty(harness.get(), "reprogrammingColumnKey",
                                                      QStringLiteral("num_reprogramacoes"));
            QVERIFY(card != nullptr);
            QVERIFY(card->property("selectedReprogrammingValues").toStringList().isEmpty());
            QTest::keyClick(&window, Qt::Key_Escape);

            QTRY_VERIFY_WITH_TIMEOUT(!onlyReprogrammed->isVisible(), 1000);
            QCOMPARE(derivation->onlyReprogrammed(), false);
            QCOMPARE(derivation->reprogrammingValues(), QStringList({QStringLiteral("1")}));
            QCOMPARE(harness->property("applyCount").toInt(), 0);

            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*openButton));
            QTRY_VERIFY_WITH_TIMEOUT(onlyReprogrammed->isVisible(), 1000);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*onlyReprogrammed));
            auto* applyButton =
                findQuickItemByProperty(window.contentItem(), "text", QStringLiteral("Aplicar"));
            QVERIFY(applyButton != nullptr);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*applyButton));

            QTRY_COMPARE_WITH_TIMEOUT(harness->property("applyCount").toInt(), 1, 1000);
            QCOMPARE(derivation->onlyReprogrammed(), true);
            QVERIFY(derivation->reprogrammingValues().isEmpty());
        }

        void reprogramming_value_selection_replaces_only_reprogrammed_draft() {
            auto repository = std::make_shared<CountingRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(derivation != nullptr);
            derivation->setOnlyReprogrammed(true);
            filters.refreshColumnValueOptionsFor(QStringLiteral("num_reprogramacoes"));
            QTRY_COMPARE_WITH_TIMEOUT(repository->reprogrammingRequests(), 1, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(
                filters.columnValueOptionsFor(QStringLiteral("num_reprogramacoes"))
                    .contains(QStringLiteral("1")),
                1000);

            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("testFilterViewModel"),
                                                     &filters);
            QQmlComponent component(&engine);
            component.setData(kReprogrammingHarness,
                              QUrl(QStringLiteral("inmemory:/ReprogrammingDraftHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 600, 500);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* openButton = findQuickItemByProperty(harnessItem, "text", QStringLiteral("..."));
            QVERIFY(openButton != nullptr);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*openButton));
            auto* popup = harness->findChild<QObject*>(QStringLiteral("reprogrammingValuePopup"));
            QVERIFY(popup != nullptr);
            QCOMPARE(popup->property("selectedOnlyReprogrammed").toBool(), true);
            QVERIFY(QMetaObject::invokeMethod(popup, "toggleSelected", Qt::DirectConnection,
                                              Q_ARG(QVariant, QVariant(QStringLiteral("1"))),
                                              Q_ARG(QVariant, QVariant(true))));
            QCOMPARE(popup->property("selectedOnlyReprogrammed").toBool(), false);
            auto* applyButton =
                findQuickItemByProperty(window.contentItem(), "text", QStringLiteral("Aplicar"));
            QVERIFY(applyButton != nullptr);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*applyButton));

            QTRY_COMPARE_WITH_TIMEOUT(harness->property("applyCount").toInt(), 1, 1000);
            QCOMPARE(derivation->onlyReprogrammed(), false);
            QCOMPARE(derivation->reprogrammingValues(), QStringList({QStringLiteral("1")}));
        }

        void reprogramming_popup_checkbox_remains_model_driven_after_click() {
            auto repository = std::make_shared<CountingRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("testFilterViewModel"),
                                                     &filters);
            QQmlComponent component(&engine);
            component.setData(kReprogrammingHarness,
                              QUrl(QStringLiteral("inmemory:/ReprogrammingReuseHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 600, 500);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* openButton = findQuickItemByProperty(harnessItem, "text", QStringLiteral("..."));
            QVERIFY(openButton != nullptr);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*openButton));
            auto* popup = harness->findChild<QObject*>(QStringLiteral("reprogrammingValuePopup"));
            QVERIFY(popup != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);

            QStringList options;
            for (int index = 0; index < 100; ++index) {
                options.push_back(QStringLiteral("value-%1").arg(index));
            }
            popup->setProperty("optionValues", options);
            popup->setProperty("selectedValues", QStringList{});
            auto* firstOption = findQuickItemByProperty(
                window.contentItem(), "objectName", QStringLiteral("reprogrammingValueOption-0"));
            QTRY_VERIFY_WITH_TIMEOUT(firstOption != nullptr && firstOption->isVisible(), 1000);
            QVERIFY(QMetaObject::invokeMethod(firstOption, "click", Qt::DirectConnection));
            QTRY_VERIFY_WITH_TIMEOUT(firstOption->property("checked").toBool(), 1000);
            popup->setProperty("selectedValues", QStringList{});
            QTRY_VERIFY_WITH_TIMEOUT(!firstOption->property("checked").toBool(), 1000);
        }

        void about_to_show_requests_distinct_values_once() {
            auto repository = std::make_shared<CountingRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("filterViewModel"), &filters);
            QQmlComponent component(&engine);
            component.setData(kAdvancedTextHarness,
                              QUrl(QStringLiteral("inmemory:/AdvancedPopupHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 600, 500);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* popup =
                harness->findChild<QObject*>(QStringLiteral("advancedTextValuePopup_situacao"));
            QVERIFY(popup != nullptr);
            QCOMPARE(repository->advancedRequests(), 0);
            QVERIFY(QMetaObject::invokeMethod(popup, "openForCurrentFilter"));
            QTRY_COMPARE_WITH_TIMEOUT(repository->advancedRequests(), 1, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(popup->property("allValues").toStringList().size(), 1, 1000);

            QVERIFY(QMetaObject::invokeMethod(popup, "close"));
            QTRY_VERIFY_WITH_TIMEOUT(!popup->property("visible").toBool(), 1000);
            QVERIFY(QMetaObject::invokeMethod(popup, "openForCurrentFilter"));
            QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!popup->property("valuesLoading").toBool(), 1000);
            QCOMPARE(repository->advancedRequests(), 1);
        }

        void distinct_value_error_is_visible_and_retryable_in_qml() {
            auto repository = std::make_shared<CountingRepository>();
            repository->failNextAdvancedRequest();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("filterViewModel"), &filters);
            QQmlComponent component(&engine);
            component.setData(kAdvancedTextHarness,
                              QUrl(QStringLiteral("inmemory:/AdvancedPopupRetryHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 600, 500);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* popup =
                harness->findChild<QObject*>(QStringLiteral("advancedTextValuePopup_situacao"));
            QVERIFY(popup != nullptr);
            QTest::ignoreMessage(QtWarningMsg,
                                 "Column value query failed: advanced values failed once");
            QVERIFY(QMetaObject::invokeMethod(popup, "openForCurrentFilter"));
            QTRY_COMPARE_WITH_TIMEOUT(repository->advancedRequests(), 1, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(popup->property("valuesError").toString(),
                                      QString("Falha ao consultar valores"), 1000);

            auto* retryButton = findOwnedQuickItemByProperty(
                popup, "objectName", QStringLiteral("advancedTextValueRetryButton_situacao"));
            QTRY_VERIFY_WITH_TIMEOUT(retryButton != nullptr && retryButton->isVisible(), 1000);
            auto* optionList = findOwnedQuickItemByProperty(
                popup, "objectName", QStringLiteral("advancedTextValueOptionList_situacao"));
            QVERIFY(optionList != nullptr);
            QVERIFY(!optionList->isVisible());
            QVERIFY(QMetaObject::invokeMethod(retryButton, "click"));

            QTRY_COMPARE_WITH_TIMEOUT(repository->advancedRequests(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(popup->property("valuesError").toString(), QString(), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(popup->property("allValues").toStringList().size(), 1, 1000);
        }

        void quick_sector_error_is_visible_and_retryable_in_qml() {
            auto repository = std::make_shared<CountingRepository>();
            repository->failNextQuickSectorRequest();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            QTest::ignoreMessage(QtWarningMsg,
                                 "Column value query failed: quick sector failed once");
            ssa::presentation::FilterPanelViewModel filters(service);
            QTRY_COMPARE_WITH_TIMEOUT(filters.sector()->property("optionsError").toString(),
                                      QString("Falha ao consultar valores"), 1000);

            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("testFilterViewModel"),
                                                     &filters);
            QQmlComponent component(&engine);
            component.setData(kQuickSectorHarness,
                              QUrl(QStringLiteral("inmemory:/QuickSectorRetryHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 1000, 100);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* retryButton = findOwnedQuickItemByProperty(
                harness.get(), "objectName", QStringLiteral("quickSectorRetryButton"));
            QTRY_VERIFY_WITH_TIMEOUT(retryButton != nullptr && retryButton->isVisible(), 1000);
            QCOMPARE(repository->quickSectorRequests(), 1);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              clickPointInWindow(*retryButton));

            QTRY_COMPARE_WITH_TIMEOUT(repository->quickSectorRequests(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(filters.sector()->property("optionsError").toString(),
                                      QString(), 1000);
            QVERIFY(filters.sector()->property("selectorValues").toStringList().contains("MEG2"));
        }

        void macro_report_virtualizes_five_thousand_rows() {
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(kMacroReportHarness,
                              QUrl(QStringLiteral("inmemory:/MacroReportHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 420, 160);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            QQuickItem* reportList = nullptr;
            QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                         reportList = harness->findChild<QQuickItem*>(
                                             QStringLiteral("macroReportList"));
                                         return reportList != nullptr;
                                     })(),
                                     1000);
            QTRY_COMPARE_WITH_TIMEOUT(reportList->property("count").toInt(), 5000, 1000);
            int delegateCount = 0;
            QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                         delegateCount = countQuickItemsByObjectName(
                                             reportList, QStringLiteral("macroReportRow"));
                                         return delegateCount > 0;
                                     })(),
                                     1000);
            QVERIFY(delegateCount < 100);
        }

        void table_double_click_selects_row_and_opens_details_once() {
            ssa::presentation::SsaTableModel tableModel("numero_ssa");
            ssa::domain::SsaPageResult page;
            page.rows.push_back(
                ssa::domain::SsaRecord{{{"numero_ssa", "202600001"}, {"situacao", "APV"}}});
            page.totalRows = 1;
            page.pageSize = 1;
            std::vector<std::string> keys{"situacao"};
            auto columns = ssa::presentation::SsaColumnDisplayCatalog{}.resolveAll(keys);
            ssa::presentation::SsaTableDisplayValues displayValues;
            displayValues.values.emplace_back(QStringLiteral("APV"));
            displayValues.rowCount = 1;
            displayValues.columnCount = 1;
            tableModel.setPage(std::move(page), std::move(keys), std::move(columns),
                               std::move(displayValues));
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("testTableModel"), &tableModel);
            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import QtQuick.Controls
                import SsaConsultaRapida

                Item {
                    id: harness
                    width: 400
                    height: 240
                    property int selectionCount: 0
                    property int detailsCount: 0
                    property alias currentRow: viewModel.currentRow

                    QtObject {
                        id: graphModel
                        property int nodeCount: 0
                    }

                    QtObject {
                        id: detailsObject
                        property var graphModel: graphModel
                    }

                    QtObject {
                        id: viewModel
                        property var tableHeaders: [{
                            key: "situacao",
                            label: "Situacao",
                            labelFull: "Situacao",
                            width: 160,
                            sorted: false,
                            filtered: false,
                            opensSam: false
                        }]
                        property var tableModel: testTableModel
                        property int currentRow: -1
                        property int totalRows: 2
                        property bool canSelectNextRow: currentRow >= 0 && currentRow < 1
                        property bool canSelectPreviousRow: currentRow > 0
                        property var details: detailsObject

                        function selectRow(row) {
                            currentRow = row;
                            harness.selectionCount += 1;
                        }

                        function sortByColumn(column) {}
                        function setFilterPanelFocusColumn(key) {}
                        function selectNextRow() {
                            selectRow(currentRow + 1);
                        }
                        function selectPreviousRow() {
                            selectRow(currentRow - 1);
                        }
                    }

                    SsaTable {
                        id: table
                        objectName: "keyboardTable"
                        anchors.fill: parent
                        viewModel: viewModel
                        density: "compact"
                        onDetailsWindowRequested: harness.detailsCount += 1
                    }

                    function firstCellCenter() {
                        return table.firstCellCenterForSmoke();
                    }
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/SsaTableDoubleClickHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 400, 240);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            QVariant firstCellCenter;
            QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                         firstCellCenter.clear();
                                         return QMetaObject::invokeMethod(
                                                    harness.get(), "firstCellCenter",
                                                    Q_RETURN_ARG(QVariant, firstCellCenter)) &&
                                                firstCellCenter.toPointF().x() >= 0;
                                     })(),
                                     1000);
            const auto cellCenter = harnessItem->mapToScene(firstCellCenter.toPointF());
            QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, cellCenter.toPoint(), 10);

            QTRY_COMPARE_WITH_TIMEOUT(harness->property("detailsCount").toInt(), 1, 1000);
            QCOMPARE(harness->property("selectionCount").toInt(), 1);
            QCOMPARE(harness->property("currentRow").toInt(), 0);

            auto* table = harness->findChild<QQuickItem*>(QStringLiteral("keyboardTable"));
            QVERIFY(table != nullptr);
            table->forceActiveFocus();
            QTRY_VERIFY_WITH_TIMEOUT(table->hasActiveFocus(), 1000);
            QTest::keyClick(&window, Qt::Key_Down);
            QCOMPARE(harness->property("currentRow").toInt(), 1);
            QTest::keyClick(&window, Qt::Key_Up);
            QCOMPARE(harness->property("currentRow").toInt(), 0);
            QTest::keyClick(&window, Qt::Key_Return);
            QCOMPARE(harness->property("selectionCount").toInt(), 3);
            QCOMPARE(harness->property("detailsCount").toInt(), 2);
        }

        void relation_navigator_activates_relation_index_from_keyboard() {
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import QtQuick.Controls
                import SsaConsultaRapida

                Item {
                    id: harness
                    width: 640
                    height: 100
                    property int loadCount: 0
                    property int loadedIndex: -1

                    QtObject {
                        id: relationModel
                        property var relations: [
                            { ssa: "202600001", role: "parent", status: "APV", kind: "Origem" },
                            { ssa: "202600002", role: "current", status: "APV", kind: "Atual" },
                            { ssa: "202600001", role: "related", status: "", kind: "Relacionada" }
                        ]
                        property int relationCount: relations.length
                        property bool relationLoading: true
                        property string relationError: "Falha de navegacao"
                        property string selectedSsaNumber: "202600001"
                        property int currentRelationIndex: 2
                        property bool canSelectPreviousRelation: currentRelationIndex > 0
                        property bool canSelectNextRelation: currentRelationIndex + 1 < relationCount
                        function selectPreviousRelation() {
                            currentRelationIndex -= 1;
                        }
                        function selectNextRelation() {
                            currentRelationIndex += 1;
                        }
                    }

                    DetailsRelationNavigator {
                        anchors.fill: parent
                        viewModel: relationModel
                        onLoadRelationRequested: relationIndex => {
                            harness.loadedIndex = relationIndex;
                            harness.loadCount += 1;
                        }
                    }
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/RelationKeyboardHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 640, 100);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* relation = findQuickItemByProperty(window.contentItem(), "objectName",
                                                     QStringLiteral("relationNode-2"));
            QTRY_VERIFY_WITH_TIMEOUT(relation != nullptr && relation->isVisible(), 1000);
            auto* status = findQuickItemByProperty(window.contentItem(), "objectName",
                                                   QStringLiteral("relationStatus"));
            QTRY_VERIFY_WITH_TIMEOUT(status != nullptr && status->isVisible(), 1000);
            QCOMPARE(status->property("text").toString(), QString("Falha de navegacao"));
            relation->forceActiveFocus();
            QTRY_VERIFY_WITH_TIMEOUT(relation->hasActiveFocus(), 1000);
            QTest::keyClick(&window, Qt::Key_Return);

            QTRY_COMPARE_WITH_TIMEOUT(harness->property("loadCount").toInt(), 1, 1000);
            QCOMPARE(harness->property("loadedIndex").toInt(), 2);
        }

        void saved_filter_activates_from_keyboard() {
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import QtQuick.Controls
                import SsaConsultaRapida

                Item {
                    id: harness
                    width: 640
                    height: 44
                    property int applyCount: 0
                    property string appliedName: ""

                    QtObject {
                        id: viewModel
                    }
                    QtObject {
                        id: filterViewModel
                        function resetFilters() {}
                    }
                    QtObject {
                        id: preferenceFlow
                        property var savedFilters: [{ name: "Filtro A" }]
                        function applySavedFilter(name) {
                            harness.appliedName = name;
                            harness.applyCount += 1;
                        }
                        function removeSavedFilter(name) {}
                    }

                    SavedFilterControls {
                        anchors.fill: parent
                        viewModel: viewModel
                        filterViewModel: filterViewModel
                        preferenceFlow: preferenceFlow
                    }
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/SavedFilterKeyboardHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 640, 44);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* savedFilter = findQuickItemByProperty(window.contentItem(), "objectName",
                                                        QStringLiteral("savedFilterTag-0"));
            QTRY_VERIFY_WITH_TIMEOUT(savedFilter != nullptr && savedFilter->isVisible(), 1000);
            savedFilter->forceActiveFocus();
            QTRY_VERIFY_WITH_TIMEOUT(savedFilter->hasActiveFocus(), 1000);
            QTest::keyClick(&window, Qt::Key_Space);

            QTRY_COMPARE_WITH_TIMEOUT(harness->property("applyCount").toInt(), 1, 1000);
            QCOMPARE(harness->property("appliedName").toString(), QString("Filtro A"));
        }

        void derivation_graph_navigates_and_activates_from_keyboard() {
            ssa::presentation::DerivadasGraphModel graphModel;
            graphModel.buildFromRelations(
                QStringLiteral("202600001"),
                {QVariantMap{{"role", "current"}, {"ssa", "202600001"}, {"status", "APV"}},
                 QVariantMap{{"role", "child"}, {"ssa", "202600002"}, {"status", "STE"}}});

            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import QtQuick.Controls
                import SsaConsultaRapida

                Item {
                    id: harness
                    width: 420
                    height: 180
                    property int clickCount: 0
                    property string clickedSsa: ""
                    required property var testGraphModel

                    DerivadasGraph {
                        id: graph
                        objectName: "keyboardGraph"
                        anchors.fill: parent
                        graphModel: harness.testGraphModel
                        onNodeClicked: ssaNumber => {
                            harness.clickedSsa = ssaNumber;
                            harness.clickCount += 1;
                        }
                    }
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/GraphKeyboardHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 420, 180);
            QVariantMap initialProperties;
            initialProperties.insert(QStringLiteral("testGraphModel"),
                                     QVariant::fromValue<QObject*>(&graphModel));
            std::unique_ptr<QObject> harness(
                component.createWithInitialProperties(initialProperties));
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* graph = harness->findChild<QQuickItem*>(QStringLiteral("keyboardGraph"));
            QVERIFY(graph != nullptr);
            graph->forceActiveFocus();
            QTRY_VERIFY_WITH_TIMEOUT(graph->hasActiveFocus(), 1000);
            QTest::keyClick(&window, Qt::Key_Right);
            QCOMPARE(graph->property("currentNodeIndex").toInt(), 1);
            QVERIFY(graph->property("contentY").toReal() > 0.0);
            QVERIFY(graph->property("contentY").toReal() <=
                    graph->property("contentHeight").toReal() - graph->height());
            QTest::keyClick(&window, Qt::Key_Return);

            QTRY_COMPARE_WITH_TIMEOUT(harness->property("clickCount").toInt(), 1, 1000);
            QCOMPARE(harness->property("clickedSsa").toString(), QString("202600002"));
        }

        void derivation_graph_exports_a_decodable_png_from_the_real_model() {
            ssa::presentation::DerivadasGraphModel graphModel;
            graphModel.buildFromRelations(
                QStringLiteral("202600001"),
                {QVariantMap{{"role", "current"}, {"ssa", "202600001"}},
                 QVariantMap{{"role", "child"}, {"ssa", "202600002"}, {"status", "STE"}}});

            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import QtQuick.Controls
                import SsaConsultaRapida

                Item {
                    required property var testGraphModel
                    width: 520
                    height: 260

                    DerivadasGraph {
                        objectName: "pngGraph"
                        anchors.fill: parent
                        graphModel: parent.testGraphModel
                    }
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/GraphPngHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QVariantMap initialProperties;
            initialProperties.insert(QStringLiteral("testGraphModel"),
                                     QVariant::fromValue<QObject*>(&graphModel));
            std::unique_ptr<QObject> harness(
                component.createWithInitialProperties(initialProperties));
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 520, 260);
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();
            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);

            auto* graph = harness->findChild<QQuickItem*>(QStringLiteral("pngGraph"));
            QVERIFY(graph != nullptr);
            QTemporaryDir outputDirectory;
            QVERIFY(outputDirectory.isValid());
            const auto outputPath = outputDirectory.filePath(QStringLiteral("graph.png"));
            QSignalSpy exportSpy(graph, SIGNAL(exportFinished(bool)));
            const QUrl outputUrl = QUrl::fromLocalFile(outputPath);
            const QVariant outputValue = outputUrl;

            QString localPath;
            QVERIFY(QMetaObject::invokeMethod(&graphModel, "localFilePath",
                                              Q_RETURN_ARG(QString, localPath),
                                              Q_ARG(QUrl, outputUrl)));
            QCOMPARE(localPath, outputPath);

            QVERIFY(QMetaObject::invokeMethod(graph, "savePng", Q_ARG(QVariant, outputValue)));

            QTRY_COMPARE_WITH_TIMEOUT(exportSpy.count(), 1, 3000);
            QCOMPARE(exportSpy.at(0).at(0).toBool(), true);
            const QImage image(outputPath);
            QVERIFY(!image.isNull());
            QVERIFY(image.width() > 0);
            QVERIFY(image.height() > 0);

            const auto unicodePath =
                outputDirectory.filePath(QStringLiteral("graph space \u00E7.png"));
            const QVariant unicodeUrl = QUrl::fromLocalFile(unicodePath);
            QVERIFY(QMetaObject::invokeMethod(graph, "savePng", Q_ARG(QVariant, unicodeUrl)));
            QTRY_COMPARE_WITH_TIMEOUT(exportSpy.count(), 2, 3000);
            QCOMPARE(exportSpy.at(1).at(0).toBool(), true);
            QVERIFY(!QImage(unicodePath).isNull());

            const std::array<QUrl, 4> nonLocalUrls = {
                QUrl{},
                QUrl(QStringLiteral("relative.png")),
                QUrl(QStringLiteral("qrc:/graph.png")),
                QUrl(QStringLiteral("https://example.invalid/graph.png")),
            };
            const QDir outputFiles(outputDirectory.path());
            const auto filesBefore = outputFiles.entryList(QDir::Files, QDir::Name);
            for (const auto& nonLocalUrl : nonLocalUrls) {
                QVERIFY(QMetaObject::invokeMethod(&graphModel, "localFilePath",
                                                  Q_RETURN_ARG(QString, localPath),
                                                  Q_ARG(QUrl, nonLocalUrl)));
                QVERIFY(localPath.isEmpty());

                const int expectedSignals = exportSpy.count() + 1;
                const QVariant nonLocalValue = nonLocalUrl;
                QVERIFY(
                    QMetaObject::invokeMethod(graph, "savePng", Q_ARG(QVariant, nonLocalValue)));
                QTRY_COMPARE_WITH_TIMEOUT(exportSpy.count(), expectedSignals, 3000);
                QCOMPARE(exportSpy.last().at(0).toBool(), false);
                QCOMPARE(outputFiles.entryList(QDir::Files, QDir::Name), filesBefore);
            }
        }

        void derivation_graph_model_delegates_windows_file_urls_to_qt() {
            ssa::presentation::DerivadasGraphModel graphModel;
            const std::array<QUrl, 2> localUrls = {
                QUrl(QStringLiteral("file:///C:/SSA/graph.png")),
                QUrl(QStringLiteral("file://server/share/graph.png")),
            };

            for (const auto& url : localUrls) {
                QVERIFY(url.isLocalFile());
                QCOMPARE(graphModel.localFilePath(url), url.toLocalFile());
            }
        }

        void filter_summary_distributes_surplus_and_preserves_natural_widths() {
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import QtQuick.Controls
                import SsaConsultaRapida

                Item {
                    width: 900
                    height: 44

                    QtObject {
                        id: filters
                        property bool excludeScaSesSte: false
                        property bool hasExclusionFilter: false
                        property var activeFilterEntries: [
                            { text: "Exec: IEE3, MEL4", kind: "column", key: "setor_executor" },
                            { text: "Sit: SEE", kind: "column", key: "situacao" }
                        ]

                        function removeActiveFilter(entry) {}
                    }

                    FilterSummaryBar {
                        id: summary
                        objectName: "filterSummaryBar"
                        anchors.fill: parent
                        filterViewModel: filters
                        searchText: "!G097"
                    }
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/FilterSummaryBarHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 900, 44);
            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();

            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            auto* summary = harness->findChild<QQuickItem*>(QStringLiteral("filterSummaryBar"));
            QVERIFY(summary != nullptr);

            const auto visibleTags = [&]() {
                QList<QQuickItem*> tags;
                QList<QQuickItem*> pending{summary};
                while (!pending.isEmpty()) {
                    auto* item = pending.takeLast();
                    pending.append(item->childItems());
                    if (item != summary && item->isVisible() &&
                        item->property("naturalWidth").isValid()) {
                        tags.append(item);
                    }
                }
                std::ranges::sort(tags, [](const QQuickItem* left, const QQuickItem* right) {
                    return left->mapToScene({0, 0}).x() < right->mapToScene({0, 0}).x();
                });
                return tags;
            };

            QTRY_COMPARE_WITH_TIMEOUT(visibleTags().size(), 3, 1000);
            const auto wideTags = visibleTags();
            std::vector<qreal> widths;
            widths.reserve(static_cast<std::size_t>(wideTags.size()));
            for (const auto* tag : wideTags) {
                const qreal naturalWidth = tag->property("naturalWidth").toReal();
                QVERIFY2(tag->width() > naturalWidth,
                         "wide summary did not distribute available width");
                widths.push_back(tag->width());
            }
            const auto [minimumWidth, maximumWidth] = std::ranges::minmax_element(widths);
            QVERIFY2(*maximumWidth - *minimumWidth <= 1.0,
                     "wide summary did not distribute final widths symmetrically");

            harnessItem->setWidth(320);
            window.setWidth(320);
            QTRY_COMPARE_WITH_TIMEOUT(summary->width(), 320, 1000);
            const auto narrowTags = visibleTags();
            QCOMPARE(narrowTags.size(), 3);
            qreal narrowContentWidth = 0;
            for (const auto* tag : narrowTags) {
                const qreal naturalWidth = tag->property("naturalWidth").toReal();
                QVERIFY2(tag->width() + 1.0 >= naturalWidth,
                         "narrow summary compressed a tag below its natural width");
                narrowContentWidth += tag->width();
            }
            QVERIFY2(narrowContentWidth > summary->width(),
                     "narrow summary did not preserve overflowing natural widths");
        }

        void theme_palette_foregrounds_meet_wcag_aa() {
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import SsaConsultaRapida

                QtObject {
                    id: harness
                    property string selectedTheme: Theme.themeName
                    property string backgroundRole: "window"
                    property string preferredRole: ""
                    readonly property string activeTheme: Theme.themeName
                    readonly property color resolvedForeground: {
                        const selectedPalette = Theme.palette;
                        const preferred = preferredRole.length > 0 ? Qt.color(selectedPalette[preferredRole]) : undefined;
                        return Theme.readableText(Qt.color(selectedPalette[backgroundRole]), preferred);
                    }

                    onSelectedThemeChanged: Theme.themeName = selectedTheme
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/ThemeForegroundHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            const QString initialTheme = harness->property("activeTheme").toString();
            QVERIFY(!initialTheme.isEmpty());
            const auto restoreTheme =
                qScopeGuard([&] { harness->setProperty("selectedTheme", initialTheme); });

            QQmlComponent paletteComponent(&engine);
            paletteComponent.setData(R"QML(
                import QtQuick
                import SsaConsultaRapida

                QtObject {
                    readonly property var allPalettes: Theme.palettes
                    readonly property var allOptions: Theme.themeOptions
                }
            )QML",
                                     QUrl(QStringLiteral("inmemory:/ThemePaletteHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(paletteComponent.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(paletteComponent.isReady(), qPrintable(paletteComponent.errorString()));

            std::unique_ptr<QObject> paletteHarness(paletteComponent.create());
            QVERIFY2(paletteHarness != nullptr, qPrintable(paletteComponent.errorString()));
            const QVariantMap palettes =
                paletteHarness->property("allPalettes").value<QJSValue>().toVariant().toMap();
            const QVariantList options =
                paletteHarness->property("allOptions").value<QJSValue>().toVariant().toList();
            QCOMPARE(palettes.size(), 39);
            QVERIFY(options.contains(QStringLiteral("system")));
            QCOMPARE(options.size(), palettes.size() + 1);

            struct ForegroundRequirement {
                QString component;
                QString backgroundRole;
                QString preferredRole;
            };
            const std::array requirements{
                ForegroundRequirement{
                    QStringLiteral("Theme generic"), QStringLiteral("accent"), {}},
                ForegroundRequirement{
                    QStringLiteral("Theme generic"), QStringLiteral("accentSoft"), {}},
                ForegroundRequirement{
                    QStringLiteral("Theme generic"), QStringLiteral("accentStrong"), {}},
                ForegroundRequirement{
                    QStringLiteral("Theme generic"), QStringLiteral("dangerSoft"), {}},
                ForegroundRequirement{
                    QStringLiteral("Theme generic"), QStringLiteral("danger"), {}},
                ForegroundRequirement{
                    QStringLiteral("Theme generic"), QStringLiteral("dangerStrong"), {}},
                ForegroundRequirement{QStringLiteral("Theme generic"), QStringLiteral("link"), {}},
                ForegroundRequirement{
                    QStringLiteral("Theme generic"), QStringLiteral("mutedText"), {}},
                ForegroundRequirement{QStringLiteral("AppComboBox highlighted"),
                                      QStringLiteral("accentSoft"), QStringLiteral("accentStrong")},
                ForegroundRequirement{QStringLiteral("ActionButton normal"),
                                      QStringLiteral("accent"), QStringLiteral("accentText")},
                ForegroundRequirement{QStringLiteral("ActionButton quiet"),
                                      QStringLiteral("panelRaised"),
                                      QStringLiteral("accentStrong")},
                ForegroundRequirement{QStringLiteral("ActionButton hover"),
                                      QStringLiteral("accentSoft"), QStringLiteral("accentText")},
                ForegroundRequirement{QStringLiteral("ActionButton down"),
                                      QStringLiteral("accentStrong"), QStringLiteral("accentText")},
                ForegroundRequirement{QStringLiteral("ActionButton danger"),
                                      QStringLiteral("dangerSoft"), QStringLiteral("dangerStrong")},
                ForegroundRequirement{QStringLiteral("ActionButton danger hover"),
                                      QStringLiteral("danger"), QStringLiteral("dangerStrong")},
                ForegroundRequirement{QStringLiteral("ActionButton danger down"),
                                      QStringLiteral("dangerStrong"),
                                      QStringLiteral("dangerStrong")},
                ForegroundRequirement{QStringLiteral("ActionButton disabled"),
                                      QStringLiteral("rowAlt"), QStringLiteral("mutedText")},
                ForegroundRequirement{QStringLiteral("FilterTabButton checked"),
                                      QStringLiteral("accent"), QStringLiteral("accentText")},
                ForegroundRequirement{QStringLiteral("FilterTabButton normal"),
                                      QStringLiteral("panelRaised"), QStringLiteral("text")},
                ForegroundRequirement{QStringLiteral("Pager included"), QStringLiteral("accent"),
                                      QStringLiteral("accentText")},
                ForegroundRequirement{QStringLiteral("Pager excluded"), QStringLiteral("mutedText"),
                                      QStringLiteral("panel")},
                ForegroundRequirement{QStringLiteral("Pager hover"), QStringLiteral("accentSoft"),
                                      QStringLiteral("text")},
                ForegroundRequirement{QStringLiteral("Pager normal"), QStringLiteral("panelRaised"),
                                      QStringLiteral("text")},
                ForegroundRequirement{QStringLiteral("ThemeDialog highlighted"),
                                      QStringLiteral("accentSoft"), QStringLiteral("accentText")},
                ForegroundRequirement{QStringLiteral("ThemeDialog normal"),
                                      QStringLiteral("surface"), QStringLiteral("text")},
                ForegroundRequirement{QStringLiteral("SsaTable header"),
                                      QStringLiteral("tableHeader"),
                                      QStringLiteral("accentStrong")},
                ForegroundRequirement{QStringLiteral("SsaTable reorder"),
                                      QStringLiteral("accentSoft"), QStringLiteral("accentStrong")},
                ForegroundRequirement{QStringLiteral("SsaTable link"), QStringLiteral("surface"),
                                      QStringLiteral("accentStrong")},
                ForegroundRequirement{QStringLiteral("SsaTable striped link"),
                                      QStringLiteral("rowAlt"), QStringLiteral("accentStrong")},
                ForegroundRequirement{QStringLiteral("SsaTable selected link"),
                                      QStringLiteral("rowSelected"),
                                      QStringLiteral("accentStrong")},
                ForegroundRequirement{QStringLiteral("SsaTable normal"), QStringLiteral("surface"),
                                      QStringLiteral("text")},
                ForegroundRequirement{QStringLiteral("SsaTable striped"), QStringLiteral("rowAlt"),
                                      QStringLiteral("text")},
                ForegroundRequirement{QStringLiteral("SsaTable selected"),
                                      QStringLiteral("rowSelected"), QStringLiteral("text")},
                ForegroundRequirement{QStringLiteral("AnalyticsChart quality"),
                                      QStringLiteral("panel"), QStringLiteral("dangerStrong")},
                ForegroundRequirement{QStringLiteral("AnalyticsChart link"),
                                      QStringLiteral("panel"), QStringLiteral("link")},
                ForegroundRequirement{QStringLiteral("AnalyticsChart link hover"),
                                      QStringLiteral("surface"), QStringLiteral("link")},
                ForegroundRequirement{QStringLiteral("AboutDialog version"),
                                      QStringLiteral("panel"), QStringLiteral("accent")},
            };

            int paletteCount = 0;
            int pythonThemeCount = 0;
            for (const QVariant& option : options) {
                const QString themeName = option.toString();
                if (themeName == QStringLiteral("system")) {
                    continue;
                }
                ++paletteCount;
                if (themeName.endsWith(QStringLiteral("py"))) {
                    ++pythonThemeCount;
                }
                QVERIFY2(palettes.contains(themeName), qPrintable("missing palette: " + themeName));
                const QVariantMap palette = palettes.value(themeName).toMap();

                harness->setProperty("selectedTheme", themeName);
                QTRY_COMPARE_WITH_TIMEOUT(harness->property("activeTheme").toString(), themeName,
                                          1000);
                for (const ForegroundRequirement& requirement : requirements) {
                    const QColor background{palette.value(requirement.backgroundRole).toString()};
                    QVERIFY2(background.isValid(), qPrintable(themeName + " invalid role: " +
                                                              requirement.backgroundRole));
                    QColor preferred;
                    if (!requirement.preferredRole.isEmpty()) {
                        preferred = QColor{palette.value(requirement.preferredRole).toString()};
                        QVERIFY2(preferred.isValid(),
                                 qPrintable(themeName + " invalid preferred role: " +
                                            requirement.preferredRole));
                    }
                    harness->setProperty("backgroundRole", requirement.backgroundRole);
                    harness->setProperty("preferredRole", requirement.preferredRole);
                    const QColor foreground{
                        harness->property("resolvedForeground").value<QColor>()};
                    QVERIFY2(
                        foreground.isValid(),
                        qPrintable(themeName + " invalid foreground for " + requirement.component));
                    QVERIFY2(
                        contrastRatio(foreground, background) >= 4.5,
                        qPrintable(themeName + " contrast failed for " + requirement.component));
                    if (preferred.isValid() && contrastRatio(preferred, background) >= 4.5) {
                        QCOMPARE(foreground, preferred);
                    }
                }
            }
            QCOMPARE(paletteCount, 39);
            QCOMPARE(pythonThemeCount, 13);

            harness->setProperty("selectedTheme", initialTheme);
            QTRY_COMPARE_WITH_TIMEOUT(harness->property("activeTheme").toString(), initialTheme,
                                      1000);
        }

        void native_theme_palettes_are_restrained_and_accessible() {
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import SsaConsultaRapida

                QtObject {
                    readonly property var allPalettes: Theme.palettes
                    readonly property var allOptions: Theme.themeOptions
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/ThemePaletteHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));
            const auto palettesValue = harness->property("allPalettes").value<QJSValue>();
            const auto optionsValue = harness->property("allOptions").value<QJSValue>();
            QVERIFY(palettesValue.isObject());
            QVERIFY(optionsValue.isArray());
            const QVariantMap palettes = palettesValue.toVariant().toMap();
            const QVariantList options = optionsValue.toVariant().toList();

            const QStringList nativeThemes{
                QStringLiteral("grayscale"),       QStringLiteral("windows7"),
                QStringLiteral("classico"),        QStringLiteral("gruvbox"),
                QStringLiteral("ssa-dark"),        QStringLiteral("dark"),
                QStringLiteral("dracula"),         QStringLiteral("solarized-dark"),
                QStringLiteral("solarized-light"), QStringLiteral("mint-light"),
                QStringLiteral("paper"),           QStringLiteral("tokyo-night"),
                QStringLiteral("catppuccin"),      QStringLiteral("nord"),
                QStringLiteral("ayu-light"),       QStringLiteral("ayu-mirage"),
                QStringLiteral("flexoki-dark"),    QStringLiteral("flexoki-light"),
                QStringLiteral("kanagawa"),        QStringLiteral("kanagawa-dragon"),
                QStringLiteral("rose-pine"),       QStringLiteral("rose-pine-moon"),
                QStringLiteral("rose-pine-dawn"),  QStringLiteral("primer-dark"),
                QStringLiteral("primer-light"),    QStringLiteral("oxocarbon-light"),
            };
            const QStringList backgroundRoles{
                QStringLiteral("window"), QStringLiteral("surface"),
                QStringLiteral("panel"),  QStringLiteral("panelRaised"),
                QStringLiteral("header"), QStringLiteral("tableHeader"),
                QStringLiteral("rowAlt"), QStringLiteral("rowSelected"),
            };
            const QStringList interactiveBackgroundRoles{
                QStringLiteral("accent"),       QStringLiteral("accentSoft"),
                QStringLiteral("dangerSoft"),   QStringLiteral("danger"),
                QStringLiteral("dangerStrong"),
            };
            QCOMPARE(nativeThemes.size(), 26);

            int pythonThemeCount = 0;
            for (const QVariant& option : options) {
                if (option.toString().endsWith(QStringLiteral("py"))) {
                    ++pythonThemeCount;
                }
            }
            QCOMPARE(pythonThemeCount, 13);

            for (const QString& themeName : nativeThemes) {
                QVERIFY2(options.contains(themeName),
                         qPrintable("missing theme option: " + themeName));
                QVERIFY2(palettes.contains(themeName), qPrintable("missing palette: " + themeName));
                const QVariantMap palette = palettes.value(themeName).toMap();
                const bool isDark = palette.value(QStringLiteral("isDark")).toBool();
                const QColor text{palette.value(QStringLiteral("text")).toString()};
                const QColor mutedText{palette.value(QStringLiteral("mutedText")).toString()};
                const QColor accent{palette.value(QStringLiteral("accent")).toString()};
                const QColor accentText{palette.value(QStringLiteral("accentText")).toString()};
                const QColor link{palette.value(QStringLiteral("link")).toString()};
                const QColor window{palette.value(QStringLiteral("window")).toString()};
                QVERIFY(text.isValid());
                QVERIFY(mutedText.isValid());
                QVERIFY(accent.isValid());
                QVERIFY(accentText.isValid());
                QVERIFY(link.isValid());
                QVERIFY(window.isValid());
                QVERIFY2(accent.hslSaturationF() <= 0.55,
                         qPrintable(themeName + " accent is too saturated"));
                QVERIFY2(contrastRatio(accentText, accent) >= 4.5,
                         qPrintable(themeName + " accent contrast is below WCAG AA"));
                QVERIFY2(contrastRatio(link, window) >= 4.5,
                         qPrintable(themeName + " link contrast is below WCAG AA"));

                const QColor panelRaised{palette.value(QStringLiteral("panelRaised")).toString()};
                for (const QString& role : interactiveBackgroundRoles) {
                    const QColor background{palette.value(role).toString()};
                    QVERIFY2(background.isValid(),
                             qPrintable(themeName + " invalid role: " + role));
                    const qreal bestContrast = std::max({contrastRatio(text, background),
                                                         contrastRatio(accentText, background),
                                                         contrastRatio(panelRaised, background)});
                    QVERIFY2(bestContrast >= 4.5,
                             qPrintable(themeName + " has no AA foreground for " + role));
                }

                for (const QString& role : backgroundRoles) {
                    const QColor background{palette.value(role).toString()};
                    QVERIFY2(background.isValid(),
                             qPrintable(themeName + " invalid role: " + role));
                    QVERIFY2(contrastRatio(text, background) >= 4.5,
                             qPrintable(themeName + " text contrast failed on " + role));
                    QVERIFY2(contrastRatio(mutedText, background) >= 4.5,
                             qPrintable(themeName + " muted contrast failed on " + role));
                    if (isDark) {
                        const qreal luminance = [&] {
                            const qreal red = linearChannel(background.redF());
                            const qreal green = linearChannel(background.greenF());
                            const qreal blue = linearChannel(background.blueF());
                            return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
                        }();
                        QVERIFY2(luminance >= 0.02,
                                 qPrintable(themeName + " contains a crushed dark surface"));
                    } else {
                        const qreal luminance = [&] {
                            const qreal red = linearChannel(background.redF());
                            const qreal green = linearChannel(background.greenF());
                            const qreal blue = linearChannel(background.blueF());
                            return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
                        }();
                        QVERIFY2(luminance <= 0.94,
                                 qPrintable(themeName + " contains a glaring light surface"));
                    }
                }

                QVERIFY2(palette.value(QStringLiteral("window")) !=
                             palette.value(QStringLiteral("surface")),
                         qPrintable(themeName + " lacks window/surface depth"));
                QVERIFY2(palette.value(QStringLiteral("panel")) !=
                             palette.value(QStringLiteral("panelRaised")),
                         qPrintable(themeName + " lacks raised-panel depth"));
            }
        }
    };

} // namespace

QTEST_MAIN(AdvancedPopupQmlTest)

#include "AdvancedPopupQmlTest.moc"
