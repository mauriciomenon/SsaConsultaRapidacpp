#include "domain/SsaTypes.h"
#include "presentation/FilterPanelDistinctValueRequestBuilder.h"
#include "presentation/FilterPanelSectorViewModel.h"
#include "presentation/FilterPanelState.h"
#include "presentation/FilterPanelViewModel.h"

#include <QtTest>

#include <mutex>
#include <vector>

namespace {

    class FilterPanelRepository final : public ssa::ports::ISsaRepository {
      public:
        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request) const override {
            return {{}, 0, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&) const override {
            return 0;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber&) const override {
            return std::nullopt;
        }

        std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest& request) const override {
            {
                const std::scoped_lock lock(mutex_);
                distinctRequests_.push_back(request);
            }
            if (request.columnKey == "setor_executor") {
                return {"MEG2", "MAM2", "OUO7"};
            }
            return {};
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer) const override {
            return {0, {}};
        }

        [[nodiscard]] std::vector<ssa::domain::DistinctValuesRequest> distinctRequests() const {
            const std::scoped_lock lock(mutex_);
            return distinctRequests_;
        }

      private:
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::DistinctValuesRequest> distinctRequests_;
    };

    class FilterPanelSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void quick_sector_options_are_loaded_from_distinct_executor_values() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            QTRY_VERIFY_WITH_TIMEOUT(filters.quickSectorOptions().contains("MEG2"), 1000);
            QVERIFY(filters.quickSectorOptions().contains("MAM2"));
            QTRY_VERIFY_WITH_TIMEOUT(!repository->distinctRequests().empty(), 1000);
            const auto distinctRequests = repository->distinctRequests();
            QCOMPARE(QString::fromStdString(distinctRequests.back().columnKey),
                     QString("setor_executor"));
            QCOMPARE(distinctRequests.back().filter.excludeScaSesSte, true);
        }

        void advanced_value_options_load_for_requested_column_without_changing_column_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("situacao");
            filters.refreshColumnValueOptionsFor("setor_executor");

            QCOMPARE(filters.columnValueOptionsLoadingFor("setor_executor"), true);
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      1000);
            QVERIFY(filters.columnValueOptionsFor("setor_executor").contains("MEG2"));
            QCOMPARE(filters.columnKey(), QString("situacao"));
        }

        void quick_sector_distinct_request_ignores_column_filters() {
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::FilterPanelDistinctValueRequestBuilder builder;

            state.addColumnFilter("situacao", "APV");

            const auto request = builder.quickSectorRequest(state);

            QCOMPARE(QString::fromStdString(request.columnKey), QString("setor_executor"));
            QVERIFY(request.filter.columnTerms.empty());
            QCOMPARE(request.filter.excludeScaSesSte, true);
        }

        void sector_submodel_owns_sector_ui_state() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(sector != nullptr);

            sector->setQuickSector("MEG2");
            sector->setExcludeScaSesSte(false);

            QCOMPARE(filters.quickSector(), QString("MEG2"));
            QCOMPARE(filters.excludeScaSesSte(), false);
            QCOMPARE(sector->quickSector(), QString("MEG2"));
            QCOMPARE(sector->excludeScaSesSte(), false);
        }

        void filter_summary_entries_are_structured_and_removable() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            filters.setQuickSector("MEG2");
            filters.setColumnFilters({{"situacao", "=APV"}});
            text->setTextFilter("setor_executor", "=MAM2");

            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 3, 1000);
            const auto entries = filters.activeFilterEntries();
            QCOMPARE(entries.size(), 3);
            QCOMPARE(entries.at(0).toMap().value("kind").toString(), QString("quick_sector"));
            QCOMPARE(entries.at(1).toMap().value("kind").toString(), QString("column"));
            QCOMPARE(entries.at(1).toMap().value("key").toString(), QString("situacao"));
            QCOMPARE(entries.at(2).toMap().value("kind").toString(), QString("advanced_text"));
            QCOMPARE(entries.at(2).toMap().value("key").toString(), QString("setor_executor"));

            QCOMPARE(filters.removeActiveFilter("column", "situacao"), true);

            QVERIFY(!filters.columnFilters().contains("situacao"));
            QCOMPARE(text->textFilter("setor_executor"), QString("=MAM2"));
            QCOMPARE(filters.quickSector(), QString("MEG2"));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(FilterPanelSmokeTest)

#include "FilterPanelSmokeTest.moc"
