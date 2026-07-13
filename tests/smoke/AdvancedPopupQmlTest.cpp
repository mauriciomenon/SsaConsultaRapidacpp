#include "domain/SsaTypes.h"
#include "ports/ISsaRepository.h"
#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableModel.h"
#include "query/SsaQueryService.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QJSValue>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QtQml/qqml.h>

#include <algorithm>
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
            if (request.limit == ssa::domain::kAdvancedDistinctValuesLimit) {
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
                                      QString("advanced values failed once"), 1000);

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
                                      QString("quick sector failed once"), 1000);

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

        void relation_navigator_activates_relation_from_keyboard() {
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
                    property string loadedSsa: ""

                    QtObject {
                        id: relationModel
                        property var relations: [
                            { ssa: "202600001", role: "current", status: "APV", kind: "Atual" },
                            { ssa: "202600002", role: "child", status: "STE", kind: "Derivada" }
                        ]
                        property int relationCount: relations.length
                        property bool relationLoading: false
                        property string relationError: ""
                        property string selectedSsaNumber: "202600001"
                        property int currentRelationIndex: 0
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
                        onLoadRelationRequested: ssaNumber => {
                            harness.loadedSsa = ssaNumber;
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
                                                     QStringLiteral("relationNode-1"));
            QTRY_VERIFY_WITH_TIMEOUT(relation != nullptr && relation->isVisible(), 1000);
            relation->forceActiveFocus();
            QTRY_VERIFY_WITH_TIMEOUT(relation->hasActiveFocus(), 1000);
            QTest::keyClick(&window, Qt::Key_Return);

            QTRY_COMPARE_WITH_TIMEOUT(harness->property("loadCount").toInt(), 1, 1000);
            QCOMPARE(harness->property("loadedSsa").toString(), QString("202600002"));
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

                    QtObject {
                        id: graphModel
                        property int nodeCount: 2
                        property real graphWidth: 900
                        property real graphHeight: 120
                        signal graphChanged
                        function rowCount() { return nodeCount; }
                        function edges() { return []; }
                        function nodeCenter(index) { return Qt.point(80 + index * 720, 70); }
                        function nodeSsa(index) { return index === 0 ? "202600001" : "202600002"; }
                        function nodeStatus(index) { return index === 0 ? "APV" : "STE"; }
                        function nodeRole(index) { return index === 0 ? "current" : "child"; }
                        function nodeIsTarget(index) { return index === 0; }
                    }

                    DerivadasGraph {
                        id: graph
                        objectName: "keyboardGraph"
                        anchors.fill: parent
                        graphModel: graphModel
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
            std::unique_ptr<QObject> harness(component.create());
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
            QVERIFY(graph->property("contentX").toReal() > 0.0);
            QVERIFY(graph->property("contentX").toReal() <=
                    graph->property("contentWidth").toReal() - graph->width());
            QTest::keyClick(&window, Qt::Key_Return);

            QTRY_COMPARE_WITH_TIMEOUT(harness->property("clickCount").toInt(), 1, 1000);
            QCOMPARE(harness->property("clickedSsa").toString(), QString("202600002"));
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

            const auto linearChannel = [](const qreal channel) {
                return channel <= 0.04045 ? channel / 12.92
                                          : std::pow((channel + 0.055) / 1.055, 2.4);
            };
            const auto luminance = [&linearChannel](const QColor& color) {
                return 0.2126 * linearChannel(color.redF()) +
                       0.7152 * linearChannel(color.greenF()) +
                       0.0722 * linearChannel(color.blueF());
            };
            const auto contrast = [&luminance](const QColor& first, const QColor& second) {
                const qreal lighter = std::max(luminance(first), luminance(second));
                const qreal darker = std::min(luminance(first), luminance(second));
                return (lighter + 0.05) / (darker + 0.05);
            };

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
                QVERIFY2(contrast(accentText, accent) >= 4.5,
                         qPrintable(themeName + " accent contrast is below WCAG AA"));
                QVERIFY2(contrast(link, window) >= 4.5,
                         qPrintable(themeName + " link contrast is below WCAG AA"));

                const QColor panelRaised{palette.value(QStringLiteral("panelRaised")).toString()};
                for (const QString& role : interactiveBackgroundRoles) {
                    const QColor background{palette.value(role).toString()};
                    QVERIFY2(background.isValid(),
                             qPrintable(themeName + " invalid role: " + role));
                    const qreal bestContrast =
                        std::max({contrast(text, background), contrast(accentText, background),
                                  contrast(panelRaised, background)});
                    QVERIFY2(bestContrast >= 4.5,
                             qPrintable(themeName + " has no AA foreground for " + role));
                }

                for (const QString& role : backgroundRoles) {
                    const QColor background{palette.value(role).toString()};
                    QVERIFY2(background.isValid(),
                             qPrintable(themeName + " invalid role: " + role));
                    QVERIFY2(contrast(text, background) >= 4.5,
                             qPrintable(themeName + " text contrast failed on " + role));
                    QVERIFY2(contrast(mutedText, background) >= 4.5,
                             qPrintable(themeName + " muted contrast failed on " + role));
                    if (isDark) {
                        QVERIFY2(luminance(background) >= 0.02,
                                 qPrintable(themeName + " contains a crushed dark surface"));
                    } else {
                        QVERIFY2(luminance(background) <= 0.94,
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
