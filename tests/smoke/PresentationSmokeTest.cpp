#include "presentation/MainViewModel.h"

#include <QSignalSpy>
#include <QtTest>

namespace {

    class FakeRepository final : public ssa::ports::ISsaRepository {
      public:
        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request) const override {
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

            QCOMPARE(model.tableModel()->rowCount(), 1);
            QCOMPARE(model.totalRows(), 1);
            QCOMPARE(model.details()->selectedSsa(), QString("202500001"));
            QVERIFY(pageSpy.count() >= 1);
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationSmokeTest)

#include "PresentationSmokeTest.moc"
