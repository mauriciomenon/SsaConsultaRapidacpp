#include "presentation/MainViewModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace {

    class FakeRepository final : public ssa::ports::ISsaRepository {
      public:
        explicit FakeRepository(std::chrono::milliseconds delay = std::chrono::milliseconds{0})
            : delay_(delay) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request) const override {
            if (delay_.count() > 0) {
                std::this_thread::sleep_for(delay_);
            }
            {
                const std::scoped_lock lock(mutex_);
                requests_.push_back(request);
            }
            ssa::domain::SsaRecord record;
            record.values["numero_ssa"] = "202500001";
            record.values["situacao"] = "APV";
            record.values["descricao_ssa"] =
                request.searchText.empty() ? "Inicial" : request.searchText;
            return {{record}, 1, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&) const override {
            return 1;
        }

        std::optional<ssa::domain::SsaRecord> recordById(const ssa::domain::SsaId&) const override {
            return std::nullopt;
        }

        std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest&) const override {
            return {};
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

      private:
        std::chrono::milliseconds delay_;
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::SsaPageRequest> requests_;
    };

    class FakeCommands final : public ssa::ports::IExternalCommandPort {
      public:
        void openSamHome() override {}
        void openSsa(const std::string&) override {}
        void openPath(const std::string&) override {}
        void exportSelection(const std::vector<std::map<std::string, std::string>>&) override {}
        void requestCommand(const std::string&,
                            const std::map<std::string, std::string>&) override {}
    };

    class PresentationSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void load_populates_table_and_details() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            QSignalSpy pageSpy(&model, &ssa::presentation::MainViewModel::pageChanged);

            model.search()->setText("Teste");
            model.apply();

            QTRY_COMPARE_WITH_TIMEOUT(model.tableModel()->rowCount(), 1, 1000);
            QCOMPARE(model.totalRows(), 1);
            QCOMPARE(model.details()->selectedSsa(), QString("202500001"));
            QVERIFY(pageSpy.count() >= 1);
            QCOMPARE(model.status()->loading(), false);
            QCOMPARE(model.tableModel()->columnLabel(0), QString("SSA"));
            QVERIFY(model.tableModel()->columnWidth(0) > 0);
        }

        void sort_by_column_updates_request_contract() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.load();
            QTRY_COMPARE_WITH_TIMEOUT(model.tableModel()->rowCount(), 1, 1000);
            model.sortByColumn(1);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);

            const auto requests = repository->requests();
            QCOMPARE(QString::fromStdString(requests.back().sort.columnKey), QString("situacao"));
            QCOMPARE(requests.back().sort.ascending, true);
            QCOMPARE(model.sortColumnKey(), QString("situacao"));
        }

        void cancel_marks_current_request_as_stale() {
            auto repository = std::make_shared<FakeRepository>(std::chrono::milliseconds{80});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.search()->setText("Primeira");
            model.apply();
            model.cancelCurrentRequest();

            QTest::qWait(160);
            QCOMPARE(model.tableModel()->rowCount(), 0);
            QCOMPARE(model.status()->message(), QString("Consulta cancelada"));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationSmokeTest)

#include "PresentationSmokeTest.moc"
