#include "domain/SsaTypes.h"
#include "ports/ISsaRepository.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableModel.h"
#include "query/SsaQueryService.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QtQml/qqml.h>

#include <atomic>
#include <memory>
#include <optional>
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

      private:
        mutable std::atomic<int> advancedRequests_{0};
    };

    [[nodiscard]] QDir repositoryRoot() {
        QDir root = QFileInfo(QString::fromUtf8(__FILE__)).dir();
        if (!root.cdUp() || !root.cdUp()) {
            qFatal("test repository root could not be resolved");
        }
        return root;
    }

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
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppTextField.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppTextField") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppCheckBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppCheckBox") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppComboBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppComboBox") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("FilterCard.qml")),
                                    "SsaConsultaRapida", 1, 0, "FilterCard") >= 0);
            QVERIFY(qmlRegisterType(
                        QUrl::fromLocalFile(components.filePath("AdvancedTextValuePopup.qml")),
                        "SsaConsultaRapida", 1, 0, "AdvancedTextValuePopup") >= 0);
            QVERIFY(qmlRegisterType(
                        QUrl::fromLocalFile(components.filePath("AdvancedTextFilterCard.qml")),
                        "SsaConsultaRapida", 1, 0, "AdvancedTextFilterCard") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("SsaTable.qml")),
                                    "SsaConsultaRapida", 1, 0, "SsaTable") >= 0);
        }

        void about_to_show_requests_distinct_values_once() {
            auto repository = std::make_shared<CountingRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("filterViewModel"), &filters);
            QQmlComponent component(&engine);
            component.setData(R"QML(
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

                        key: "situacao"
                        label: "Situacao"
                        operatorModes: [{ label: "=", mode: "equals" }]
                        allValues: loadedValues
                        visibleValues: loadedValues
                        valuesLoading: loadedValuesLoading
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
            )QML",
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
                        property var details: detailsObject

                        function selectRow(row) {
                            currentRow = row;
                            harness.selectionCount += 1;
                        }

                        function sortByColumn(column) {}
                        function setFilterPanelFocusColumn(key) {}
                    }

                    SsaTable {
                        id: table
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
        }
    };

} // namespace

QTEST_MAIN(AdvancedPopupQmlTest)

#include "AdvancedPopupQmlTest.moc"
