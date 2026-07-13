#include "PresentationSmokeFakes.h"

#include "application/SsaWorkflowService.h"
#include "domain/SsaTypes.h"
#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/DerivadasGraphModel.h"
#include "presentation/DetailsViewModel.h"
#include "presentation/ExportViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/MainPreferenceFlowCoordinator.h"
#include "presentation/MainViewModel.h"
#include "presentation/PageQueryCoordinator.h"
#include "presentation/SsaRecordValueFormatter.h"
#include "presentation/StatusViewModel.h"

#include <QChar>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QThread>
#include <QUrl>
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

    using ssa::tests::presentation_smoke::CapturingDerivadasPort;
    using ssa::tests::presentation_smoke::CapturingImportPort;
    using ssa::tests::presentation_smoke::CapturingMaintenancePort;
    using ssa::tests::presentation_smoke::FakeCommands;
    using ssa::tests::presentation_smoke::FakePreferences;
    using ssa::tests::presentation_smoke::FakeRepository;
    using ssa::tests::presentation_smoke::FakeRepositoryConfig;

    class ThreadCapturingCommands final : public ssa::ports::IExternalCommandPort {
      public:
        ssa::ports::ExternalCommandResult execute(const ssa::ports::ExternalCommand&) override {
            const std::scoped_lock lock(mutex_);
            executedThread_ = QThread::currentThread();
            return {ssa::ports::ExternalCommandStatus::Succeeded, "ok"};
        }

        [[nodiscard]] QThread* executedThread() const {
            const std::scoped_lock lock(mutex_);
            return executedThread_;
        }

      private:
        mutable std::mutex mutex_;
        QThread* executedThread_{nullptr};
    };

    class BlockingCancelableExportPort final : public ssa::ports::IExportPort {
      public:
        ssa::ports::WorkflowResult
        exportFilteredList(const ssa::ports::ExportFilteredListRequest&,
                           const std::stop_token stopToken = {}) override {
            started_.store(true, std::memory_order_release);
            while (!stopToken.stop_requested()) {
                std::this_thread::yield();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
            finished_.store(true, std::memory_order_release);
            return {ssa::ports::WorkflowStatus::Failed, "export canceled"};
        }

        [[nodiscard]] bool started() const {
            return started_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool finished() const {
            return finished_.load(std::memory_order_acquire);
        }

      private:
        std::atomic_bool started_{false};
        std::atomic_bool finished_{false};
    };

    class SlowCancelableRepository final : public ssa::ports::ISsaRepository {
      public:
        explicit SlowCancelableRepository(const bool failAfterStop = false)
            : failAfterStop_(failAfterStop) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        const std::stop_token stopToken = {}) const override {
            if (request.searchText == "first") {
                firstStarted_.store(true, std::memory_order_release);
                while (!stopToken.stop_requested()) {
                    std::this_thread::yield();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{300});
                firstFinished_.store(true, std::memory_order_release);
                if (failAfterStop_) {
                    throw std::runtime_error("query failed after stop");
                }
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            secondStarted_.store(true, std::memory_order_release);
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

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest&,
                                                std::stop_token = {}) const override {
            return {};
        }

        std::size_t maxValueLength(std::string_view, std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] bool firstStarted() const {
            return firstStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool firstFinished() const {
            return firstFinished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool secondStarted() const {
            return secondStarted_.load(std::memory_order_acquire);
        }

      private:
        bool failAfterStop_{false};
        mutable std::atomic_bool firstStarted_{false};
        mutable std::atomic_bool firstFinished_{false};
        mutable std::atomic_bool secondStarted_{false};
    };

    class DetailsRelationRepository final : public ssa::ports::ISsaRepository {
      public:
        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token) const override {
            return {{}, 0, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token) const override {
            return 0;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber& number,
                          std::stop_token = {}) const override {
            const auto found = records_.find(number.value());
            if (found == records_.end()) {
                return std::nullopt;
            }
            return found->second;
        }

        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber& number,
                         std::stop_token = {}) const override {
            const auto found = children_.find(number.value());
            if (found == children_.end()) {
                return {};
            }
            return found->second;
        }

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest&,
                                                std::stop_token) const override {
            return {};
        }

        [[nodiscard]] std::size_t maxValueLength(std::string_view,
                                                 std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {0, {}};
        }

        void setRecord(std::string number, ssa::domain::SsaRecord record) {
            records_.insert_or_assign(std::move(number), std::move(record));
        }

        void setChildren(std::string number, std::vector<ssa::domain::SsaDerivadaEntry> children) {
            children_.insert_or_assign(std::move(number), std::move(children));
        }

      private:
        std::map<std::string, ssa::domain::SsaRecord> records_;
        std::map<std::string, std::vector<ssa::domain::SsaDerivadaEntry>> children_;
    };

    class SlowDetailsRepository final : public ssa::ports::ISsaRepository {
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
        derivadasDiretas(const ssa::domain::SsaNumber& number,
                         const std::stop_token stopToken = {}) const override {
            if (number.value() == "error") {
                throw std::runtime_error("relation query failed");
            }
            if (number.value() == "202500001") {
                firstStarted_.store(true, std::memory_order_release);
                const auto deadline =
                    std::chrono::steady_clock::now() + std::chrono::milliseconds{500};
                while (!stopToken.stop_requested() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::yield();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{200});
                firstFinished_.store(true, std::memory_order_release);
                if (stopToken.stop_requested()) {
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled));
                }
                return {{"old-child", "APV"}};
            }
            secondStarted_.store(true, std::memory_order_release);
            return {{"new-child", "SES"}};
        }

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest&,
                                                std::stop_token = {}) const override {
            return {};
        }

        std::size_t maxValueLength(std::string_view, std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] bool firstStarted() const {
            return firstStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool firstFinished() const {
            return firstFinished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool secondStarted() const {
            return secondStarted_.load(std::memory_order_acquire);
        }

      private:
        mutable std::atomic_bool firstStarted_{false};
        mutable std::atomic_bool firstFinished_{false};
        mutable std::atomic_bool secondStarted_{false};
    };

    class SlowRelationNavigationRepository final : public ssa::ports::ISsaRepository {
      public:
        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token = {}) const override {
            return {{}, 0, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token = {}) const override {
            return 0;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber& number,
                          const std::stop_token stopToken = {}) const override {
            if (number.value() == "old") {
                firstStarted_.store(true, std::memory_order_release);
                const auto deadline =
                    std::chrono::steady_clock::now() + std::chrono::milliseconds{500};
                while (!stopToken.stop_requested() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::yield();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{200});
                firstFinished_.store(true, std::memory_order_release);
                if (stopToken.stop_requested()) {
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled));
                }
            } else {
                secondStarted_.store(true, std::memory_order_release);
            }
            return ssa::domain::SsaRecord{{{"numero_ssa", number.value()}, {"situacao", "APV"}}};
        }

        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return {};
        }

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest&,
                                                std::stop_token = {}) const override {
            return {};
        }

        std::size_t maxValueLength(std::string_view, std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] bool firstStarted() const {
            return firstStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool firstFinished() const {
            return firstFinished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool secondStarted() const {
            return secondStarted_.load(std::memory_order_acquire);
        }

      private:
        mutable std::atomic_bool firstStarted_{false};
        mutable std::atomic_bool firstFinished_{false};
        mutable std::atomic_bool secondStarted_{false};
    };

    class PresentationSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void date_text_formatter_keeps_only_day_month_year() {
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "2026-04-13 11:21:01", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("13/04/2026"));
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "2026-04-13T12:26:00", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("13/04/2026"));
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "sem data", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("sem data"));
        }

        void status_view_model_query_complete_messages_avoid_count_duplication() {
            ssa::presentation::StatusViewModel status;
            // Single page result: generic completion message, no count in the
            // message (count is shown separately in the status pill).
            status.setQueryComplete(10, 10, 1, 1);
            QCOMPARE(status.message(), QString("Consulta concluida"));
            QVERIFY(!status.message().contains("SSAs"));

            // Multipage: show page info instead of the count.
            status.setQueryComplete(10, 250, 1, 25);
            QCOMPARE(status.message(), QString("Pagina 1 de 25"));

            // Empty result.
            status.setQueryComplete(0, 100, 1, 10);
            QCOMPARE(status.message(), QString("Nenhum resultado"));
        }

        void load_populates_table_and_allows_details_selection() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            QSignalSpy pageSpy(model.browse(), &ssa::presentation::BrowseViewModel::pageChanged);

            model.browse()->search()->setText("Teste");
            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QCOMPARE(model.browse()->totalRows(), 1);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));
            QVERIFY(pageSpy.size() >= 1);
            QCOMPARE(model.browse()->status()->loading(), false);
            QCOMPARE(model.browse()->tableModel()->columnLabel(0), QString("No SSA"));
            QVERIFY(model.browse()->tableModel()->columnWidth(0) > 0);
            QCOMPARE(model.browse()->tableModel()->ssaNumberAt(0), QString("202500001"));
            QCOMPARE(model.browse()->tableModel()->ssaNumberAt(99), QString());
        }

        void details_relation_current_carries_status_from_record() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);

            const auto relations = model.browse()->details()->relations();
            QVERIFY(!relations.empty());
            // The first relation is the Current node and must carry the
            // situacao status from the loaded record (FakeRepository sets APV).
            const auto current = relations.at(0).toMap();
            QCOMPARE(current.value("kind").toString(), QString("Atual"));
            QCOMPARE(current.value("status").toString(), QString("APV"));
            QCOMPARE(current.value("ssa").toString(), QString("202500001"));
        }

        void details_graph_model_rebuilds_after_load() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QVERIFY(model.browse()->details()->graphModel()->rowCount() == 0);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);

            // After load, the graph model must contain at least the target node.
            QVERIFY(model.browse()->details()->graphModel()->rowCount() >= 1);
            QCOMPARE(model.browse()->details()->graphModel()->target(), QString("202500001"));
        }

        void details_fields_keep_python_priority_order() {
            ssa::presentation::DetailsViewModel details;
            details.setRecord(ssa::domain::SsaRecord{{{"id", "3802"},
                                                      {"responsavel_execucao", "DANILO NADAL"},
                                                      {"descricao_ssa", "Descricao longa"},
                                                      {"numero_ssa", "202500003"},
                                                      {"qtd_derivadas", "2"},
                                                      {"situacao", "APV"},
                                                      {"solicitante", "CARLOS ORTIZ"},
                                                      {"localizacao_codigo", "T075Q002"},
                                                      {"setor_executor", "MEL4"},
                                                      {"responsavel_programacao", "DANILO NADAL"},
                                                      {"setor_emissor", "IEE3"}}});

            auto* fields = details.fields();
            QVERIFY(fields != nullptr);
            QCOMPARE(fields->rowCount(), 10);
            QCOMPARE(
                fields->data(fields->index(0, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("No SSA"));
            QCOMPARE(
                fields->data(fields->index(0, 0), ssa::presentation::DetailsFieldsModel::KeyRole)
                    .toString(),
                QString("numero_ssa"));
            QCOMPARE(
                fields->data(fields->index(1, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Sit."));
            QCOMPARE(
                fields->data(fields->index(2, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Loc."));
            QCOMPARE(
                fields->data(fields->index(3, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Emis."));
            QCOMPARE(
                fields->data(fields->index(4, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Exec."));
            QCOMPARE(
                fields->data(fields->index(5, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Qtd Der."));
            QCOMPARE(
                fields->data(fields->index(5, 0), ssa::presentation::DetailsFieldsModel::KeyRole)
                    .toString(),
                QString("qtd_derivadas"));
            QCOMPARE(
                fields->data(fields->index(8, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Resp. Programacao"));
            QCOMPARE(
                fields->data(fields->index(9, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Resp. Execucao"));
            for (int row = 0; row < fields->rowCount(); ++row) {
                QVERIFY(fields
                            ->data(fields->index(row, 0),
                                   ssa::presentation::DetailsFieldsModel::LabelRole)
                            .toString() != QString("ID"));
            }
        }

        void details_relation_navigation_indices_and_flags() {
            // FakeRepository returns nullopt for recordBySsaNumber, so
            // the async relation load will not change the record; but the index/flags
            // logic must still respond. Use a DetailsViewModel with a service.
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            // Build a record with an ancestor so relationCount >= 2.
            ssa::domain::SsaRecord record{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}};
            details.setRecord(record);

            QCOMPARE(details.relationCount(), 2);
            QCOMPARE(details.currentRelationIndex(), 1);
            QVERIFY(details.canSelectPreviousRelation());
            QVERIFY(!details.canSelectNextRelation());

            details.selectPreviousRelation();
            // FakeRepository returns nullopt so the record does not change,
            // but the navigation index advances.
            QCOMPARE(details.currentRelationIndex(), 0);
            QVERIFY(!details.canSelectPreviousRelation());
            QVERIFY(details.canSelectNextRelation());

            details.selectNextRelation();
            QCOMPARE(details.currentRelationIndex(), 1);
            QVERIFY(!details.canSelectNextRelation());

            // Out-of-range calls are no-ops.
            details.selectNextRelation();
            QCOMPARE(details.currentRelationIndex(), 1);
            details.selectPreviousRelation();
            details.selectPreviousRelation();
            QCOMPARE(details.currentRelationIndex(), 0);
        }

        void details_load_relation_clamps_index_after_successful_shorter_chain_load() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setRecord("202500001", ssa::domain::SsaRecord{{{"numero_ssa", "202500001"},
                                                                       {"situacao", "APV"}}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}});

            QCOMPARE(details.relationCount(), 2);
            details.selectPreviousRelation();

            QTRY_COMPARE_WITH_TIMEOUT(details.selectedSsa(), QString("202500001"), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(details.relationCount(), 1, 1000);
            QCOMPARE(details.currentRelationIndex(), 0);
            QVERIFY(!details.canSelectNextRelation());
            QVERIFY(!details.canSelectPreviousRelation());
        }

        void details_relation_query_is_async_latest_wins_and_discards_stale_result() {
            auto repository = std::make_shared<SlowDetailsRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);
            QElapsedTimer elapsed;
            elapsed.start();

            details.setRecord(
                ssa::domain::SsaRecord{{{"numero_ssa", "202500001"}, {"situacao", "APV"}}});

            QVERIFY(elapsed.elapsed() < 100);
            QVERIFY(details.relationLoading());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            details.setRecord(
                ssa::domain::SsaRecord{{{"numero_ssa", "202500002"}, {"situacao", "APV"}}});

            QTRY_VERIFY_WITH_TIMEOUT(repository->secondStarted(), 1000);
            QVERIFY(!repository->firstFinished());
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.relationError(), QString());
            QCOMPARE(details.selectedSsaNumber(), QString("202500002"));
            QCOMPARE(details.relationCount(), 2);
            QCOMPARE(details.relations().at(1).toMap().value("ssa").toString(),
                     QString("new-child"));
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QCOMPARE(details.selectedSsaNumber(), QString("202500002"));
            QCOMPARE(details.relationCount(), 2);
        }

        void details_relation_query_exposes_failure_without_erasing_local_record() {
            auto repository = std::make_shared<SlowDetailsRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(
                ssa::domain::SsaRecord{{{"numero_ssa", "error"}, {"situacao", "APV"}}});

            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.relationError(), QString("relation query failed"));
            QCOMPARE(details.selectedSsaNumber(), QString("error"));
            QCOMPARE(details.relationCount(), 1);
        }

        void details_relation_navigation_load_is_async_latest_wins() {
            auto repository = std::make_shared<SlowRelationNavigationRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);
            QElapsedTimer elapsed;
            elapsed.start();

            details.requestLoadBySsaNumber(QStringLiteral("old"));

            QVERIFY(elapsed.elapsed() < 100);
            QVERIFY(details.relationLoading());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            details.requestLoadBySsaNumber(QStringLiteral("new"));

            QTRY_VERIFY_WITH_TIMEOUT(repository->secondStarted(), 1000);
            QVERIFY(!repository->firstFinished());
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.selectedSsaNumber(), QString("new"));
            QCOMPARE(details.relationError(), QString());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QCOMPARE(details.selectedSsaNumber(), QString("new"));
        }

        void details_window_model_starts_loading_without_blocking_the_gui_thread() {
            auto repository = std::make_shared<SlowRelationNavigationRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            QElapsedTimer elapsed;
            elapsed.start();

            auto* details = model.browse()->createDetailsWindowModel(QStringLiteral("old"), &model);

            QVERIFY(elapsed.elapsed() < 100);
            QVERIFY(details->relationLoading());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);
        }

        void search_apply_signal_reloads_table() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->search()->setText("bomba");
            model.browse()->search()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(QString::fromStdString(repository->requests().back().searchText),
                     QString("bomba"));
        }

        void details_navigation_walks_next_then_prev_within_page() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{3}, .rowCount = std::size_t{3}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QSignalSpy rowSpy(model.browse(),
                              &ssa::presentation::BrowseViewModel::currentRowChanged);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 3, 1000);

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));
            QVERIFY(model.browse()->canSelectNextRow());
            QVERIFY(!model.browse()->canSelectPreviousRow());
            QCOMPARE(rowSpy.size(), 1);

            model.browse()->selectNextRow();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 1, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500002"));
            QVERIFY(model.browse()->canSelectNextRow());
            QVERIFY(model.browse()->canSelectPreviousRow());

            model.browse()->selectNextRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 2, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500003"));
            QVERIFY(!model.browse()->canSelectNextRow());
            QVERIFY(model.browse()->canSelectPreviousRow());

            model.browse()->selectNextRow();
            QVERIFY(model.browse()->currentRow() == 2);

            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 1, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500002"));

            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));

            model.browse()->selectPreviousRow();
            QVERIFY(model.browse()->currentRow() == 0);
        }

        void selection_flow_open_ssa_dispatches_external_command() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QMetaObject::invokeMethod(model.selectionFlow(), "openSsa", Qt::QueuedConnection,
                                      Q_ARG(QString, QString("202500002")));
            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            QCOMPARE(commands->commands().front().kind, ssa::ports::ExternalCommandKind::OpenSsa);
            QCOMPARE(commands->commands().front().parameters.at("ssa_number"),
                     std::string("202500002"));
        }

        void selection_flow_open_ssa_ignores_empty_input() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QMetaObject::invokeMethod(model.selectionFlow(), "openSsa", Qt::QueuedConnection,
                                      Q_ARG(QString, QString("   ")));
            QTest::qWait(50);
            QCOMPARE(commands->commands().size(), std::size_t{0});
        }

        void derivadas_graph_model_builds_target_ancestor_and_child_layout() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(
                QVariantMap{{"kind", "Atual"}, {"ssa", "202500002"}, {"status", "APV"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});
            relations.push_back(
                QVariantMap{{"kind", "Relacionada"}, {"ssa", "202500003"}, {"status", "STE"}});

            QSignalSpy spy(&model, &ssa::presentation::DerivadasGraphModel::graphChanged);
            model.buildFromRelations(QStringLiteral("202500002"), relations);

            QCOMPARE(model.target(), QString("202500002"));
            QCOMPARE(model.rowCount(), 3);
            QCOMPARE(spy.size(), 1);
            QVERIFY(model.graphWidth() > 0);
            QVERIFY(model.graphHeight() > 0);

            // Ancestor first (depth 0), target second (depth 1), child third.
            QCOMPARE(model.nodeSsa(0), QString("202500001"));
            QCOMPARE(model.nodeIsTarget(0), false);
            QCOMPARE(model.nodeRole(0), QString("parent"));
            QCOMPARE(model.nodeSsa(1), QString("202500002"));
            QCOMPARE(model.nodeIsTarget(1), true);
            QCOMPARE(model.nodeRole(1), QString("current"));
            QCOMPARE(model.nodeStatus(1), QString("APV"));
            QCOMPARE(model.nodeSsa(2), QString("202500003"));
            QCOMPARE(model.nodeRole(2), QString("related"));
            QCOMPARE(model.nodeStatus(2), QString("STE"));

            const auto edges = model.edges();
            QCOMPARE(edges.size(), 2);
            // First edge: ancestor -> target (solid)
            QCOMPARE(edges.at(0).toMap().value("dashed"), false);
            // Second edge: target -> related (dashed)
            QCOMPARE(edges.at(1).toMap().value("dashed"), true);

            const auto svg = model.svg();
            QVERIFY(svg.contains(QStringLiteral("<svg")));
            QVERIFY(svg.contains(QStringLiteral("<path")));
            QVERIFY(svg.contains(QStringLiteral("202500002")));
            QVERIFY(svg.contains(QStringLiteral("APV")));
            QVERIFY(svg.contains(QStringLiteral("stroke-dasharray")));
        }

        void derivadas_graph_model_uses_details_view_model_relation_roles() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setChildren("202500003", {{"202500004", "STE"}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}});

            QTRY_COMPARE_WITH_TIMEOUT(details.relationCount(), 3, 1000);
            const auto relations = details.relations();
            QCOMPARE(relations.size(), 3);
            QCOMPARE(relations.at(0).toMap().value("kind").toString(), QString("Origem"));
            QCOMPARE(relations.at(0).toMap().value("role").toString(), QString("parent"));
            QCOMPARE(relations.at(1).toMap().value("kind").toString(), QString("Atual"));
            QCOMPARE(relations.at(1).toMap().value("role").toString(), QString("current"));
            QCOMPARE(relations.at(2).toMap().value("kind").toString(), QString("Derivada"));
            QCOMPARE(relations.at(2).toMap().value("role").toString(), QString("child"));

            const auto* graph = details.graphModel();
            QCOMPARE(graph->target(), QString("202500003"));
            QCOMPARE(graph->rowCount(), 3);
            QCOMPARE(graph->nodeSsa(0), QString("202500001"));
            QCOMPARE(graph->nodeIsTarget(0), false);
            QCOMPARE(graph->nodeRole(0), QString("parent"));
            QCOMPARE(graph->nodeSsa(1), QString("202500003"));
            QCOMPARE(graph->nodeIsTarget(1), true);
            QCOMPARE(graph->nodeRole(1), QString("current"));
            QCOMPARE(graph->nodeStatus(1), QString("APV"));
            QCOMPARE(graph->nodeSsa(2), QString("202500004"));
            QCOMPARE(graph->nodeRole(2), QString("child"));
            QCOMPARE(graph->nodeStatus(2), QString("STE"));
            QCOMPARE(graph->edges().size(), 2);
            QCOMPARE(graph->edges().at(0).toMap().value("dashed"), false);
            QCOMPARE(graph->edges().at(1).toMap().value("dashed"), false);
        }

        void derivadas_graph_model_clears_on_empty_target() {
            ssa::presentation::DerivadasGraphModel model;
            model.buildFromRelations(QStringLiteral("202500002"), {});
            QCOMPARE(model.rowCount(), 1);

            model.buildFromRelations(QString(), {});
            QCOMPARE(model.rowCount(), 0);
            QCOMPARE(model.target(), QString());
            QCOMPARE(model.graphWidth(), 0.0);
        }

        void derivadas_graph_model_layout_x_advances_with_depth() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"kind", "Atual"}, {"ssa", "202500003"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500002"}});
            relations.push_back(QVariantMap{{"kind", "Relacionada"}, {"ssa", "202500004"}});

            model.buildFromRelations(QStringLiteral("202500003"), relations);
            // 2 ancestors (depth 0,1) + target (depth 2) + 1 child (depth 3)
            QCOMPARE(model.rowCount(), 4);

            // x increases with depth: ancestor0 < ancestor1 < target < child
            const auto xAncestor0 = model.nodeCenter(0).x();
            const auto xAncestor1 = model.nodeCenter(1).x();
            const auto xTarget = model.nodeCenter(2).x();
            const auto xChild = model.nodeCenter(3).x();
            QVERIFY(xAncestor0 < xAncestor1);
            QVERIFY(xAncestor1 < xTarget);
            QVERIFY(xTarget < xChild);

            // ancestors and target stay on the same row; child relations are one level below.
            QCOMPARE(model.nodeCenter(0).y(), model.nodeCenter(1).y());
            QCOMPARE(model.nodeCenter(1).y(), model.nodeCenter(2).y());
            QVERIFY(model.nodeCenter(3).y() > model.nodeCenter(2).y());
        }

        void derivadas_graph_model_fans_many_children_below_target() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"role", "current"}, {"ssa", "202500100"}});
            for (int index = 0; index < 10; ++index) {
                relations.push_back(QVariantMap{
                    {"role", "child"}, {"ssa", QStringLiteral("20250010%1").arg(index + 1)}});
            }

            model.buildFromRelations(QStringLiteral("202500100"), relations);

            QCOMPARE(model.rowCount(), 11);
            const auto targetY = model.nodeCenter(0).y();
            const auto targetX = model.nodeCenter(0).x();
            for (int row = 1; row < model.rowCount(); ++row) {
                QVERIFY(model.nodeCenter(row).y() > targetY);
                QVERIFY(model.nodeCenter(row).x() > targetX);
            }
            QVERIFY(model.nodeCenter(1).x() < model.nodeCenter(4).x());
            QVERIFY(model.nodeCenter(5).x() == model.nodeCenter(1).x());
            QVERIFY(model.nodeCenter(5).y() > model.nodeCenter(1).y());

            const auto edges = model.edges();
            QCOMPARE(edges.size(), 10);
            for (const auto& edgeValue : edges) {
                const auto edge = edgeValue.toMap();
                QCOMPARE(edge.value("from").toString(), QString("202500100"));
                QVERIFY(edge.value("to").toString().startsWith(QString("20250010")));
                QVERIFY(edge.value("fromX").toReal() > targetX);
                QVERIFY(edge.value("toX").toReal() > edge.value("fromX").toReal());
                QVERIFY(edge.value("toY").toReal() > edge.value("fromY").toReal());
                QVERIFY(edge.value("routeX").toReal() > edge.value("fromX").toReal());
                QVERIFY(edge.value("routeX").toReal() < edge.value("toX").toReal());
            }
        }

        void derivadas_graph_model_invalid_index_returns_empty() {
            ssa::presentation::DerivadasGraphModel model;
            model.buildFromRelations(QStringLiteral("202500001"),
                                     {QVariantMap{{"kind", "Atual"}, {"ssa", "202500001"}}});

            QCOMPARE(model.nodeSsa(-1), QString());
            QCOMPARE(model.nodeStatus(-1), QString());
            QCOMPARE(model.nodeSsa(99), QString());
            QCOMPARE(model.nodeStatus(99), QString());
            QCOMPARE(model.nodeIsTarget(-1), false);
            QVERIFY(model.nodeCenter(-1).isNull());
        }

        void derivadas_graph_model_dedupes_repeated_relations() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"kind", "Atual"}, {"ssa", "202500002"}});
            // Same ancestor twice
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});

            model.buildFromRelations(QStringLiteral("202500002"), relations);
            // Target + 1 deduped ancestor = 2 nodes (not 3)
            QCOMPARE(model.rowCount(), 2);
            QCOMPARE(model.edges().size(), 1);
        }

        void details_navigation_walks_across_pages_next_then_prev() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{25}, .rowCount = std::size_t{10}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->setPageSize(10);
            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 10, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->pageCount(), 3);

            // Walk to last row of page 1
            for (int i = 0; i < 9; ++i) {
                model.browse()->selectNextRow();
            }
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 9, 1000);
            QVERIFY(model.browse()->canSelectNextRow());
            QCOMPARE(model.browse()->pageNumber(), 1);

            // Cross to page 2 -> auto-select first row
            model.browse()->selectNextRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));

            // Previous from first row of page 2 -> back to page 1 last row
            QVERIFY(model.browse()->canSelectPreviousRow());
            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 1, 2000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 9, 2000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500010"));
        }

        void sort_by_column_updates_request_contract() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            model.browse()->sortByColumn(1);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);

            const auto requests = repository->requests();
            QCOMPARE(QString::fromStdString(requests.back().sort.columnKey), QString("situacao"));
            QCOMPARE(requests.back().sort.ascending, true);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
        }

        void sort_cycle_ascends_descends_then_clears() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), true);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), false);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString(""));
            QCOMPARE(model.browse()->sortAscending(), false);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), true);
        }

        void sort_by_column_resets_page_and_saves_preferences() {
            const auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{21}});
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->setPageSize(10);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageCount(), 3, 1000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().back().pageIndex, std::size_t{1},
                                      1000);

            model.browse()->sortByColumn(1);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().back().pageIndex, std::size_t{0},
                                      1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 1, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(QString::fromStdString(preferences->snapshot().sortColumnKey),
                                      QString("situacao"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().sortAscending, true, 1000);
        }

        void table_headers_expose_sort_indicator_state() {
            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            model.browse()->sortByColumn(1);

            const auto headers = model.browse()->tableHeaders();
            QVERIFY(headers.size() > 1);
            const auto sortedHeader = headers[1].toMap();
            QCOMPARE(sortedHeader.value("key").toString(), QString("situacao"));
            QCOMPARE(sortedHeader.value("sorted").toBool(), true);
            QCOMPARE(sortedHeader.value("sortAscending").toBool(), true);

            const auto unsortedHeader = headers[0].toMap();
            QCOMPARE(unsortedHeader.value("sorted").toBool(), false);

            model.browse()->filters()->setColumnFilters({{"situacao", "APV"}});
            const auto filteredHeaders = model.browse()->tableHeaders();
            const auto filteredHeader = filteredHeaders[1].toMap();
            QCOMPARE(filteredHeader.value("key").toString(), QString("situacao"));
            QCOMPARE(filteredHeader.value("filtered").toBool(), true);
            const auto unfilteredHeader = filteredHeaders[0].toMap();
            QCOMPARE(unfilteredHeader.value("filtered").toBool(), false);

            model.browse()->sortByColumn(1);
            const auto descendingHeaders = model.browse()->tableHeaders();
            const auto descendingHeader = descendingHeaders[1].toMap();
            QCOMPARE(descendingHeader.value("key").toString(), QString("situacao"));
            QCOMPARE(descendingHeader.value("sorted").toBool(), true);
            QCOMPARE(descendingHeader.value("sortAscending").toBool(), false);
        }

        void next_page_reaches_final_page() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{21}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->setPageSize(10);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageCount(), 3, 1000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 3000);
            QCOMPARE(repository->requests().back().pageIndex, std::size_t{1});
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{3}, 3000);
            QCOMPARE(repository->requests().back().pageIndex, std::size_t{2});
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 3, 1000);

            QCOMPARE(model.browse()->pageNumber(), 3);
            QCOMPARE(model.browse()->pageCount(), 3);
            QCOMPARE(repository->requests().back().pageIndex, std::size_t{2});
        }

        void cancel_marks_current_request_as_stale() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.delay = std::chrono::milliseconds{80}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->search()->setText("Primeira");
            model.browse()->apply();
            model.browse()->cancelCurrentRequest();

            QTest::qWait(160);
            QCOMPARE(model.browse()->tableModel()->rowCount(), 0);
            QCOMPARE(model.browse()->status()->message(), QString("Consulta cancelada"));
        }

        void page_query_starts_latest_request_before_canceled_worker_finishes() {
            auto repository = std::make_shared<SlowCancelableRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int succeededCount = 0;
            int canceledCount = 0;
            int failedCount = 0;
            std::string completedSearch;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                    [&](const ssa::presentation::PageQueryResult&,
                        const ssa::domain::SsaPageRequest& request) {
                        ++succeededCount;
                        completedSearch = request.searchText;
                    });
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::canceled, this,
                    [&] { ++canceledCount; });
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::failed, this,
                    [&](const QString&) { ++failedCount; });
            ssa::domain::SsaPageRequest firstRequest;
            firstRequest.searchText = "first";

            coordinator.run(firstRequest);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            auto secondRequest = firstRequest;
            secondRequest.searchText = "second";
            coordinator.run(secondRequest);

            QTRY_VERIFY_WITH_TIMEOUT(repository->secondStarted(), 150);
            QVERIFY(!repository->firstFinished());
            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 1, 1000);
            QCOMPARE(completedSearch, std::string{"second"});
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QCoreApplication::processEvents();
            QCOMPARE(succeededCount, 1);
            QCOMPARE(canceledCount, 0);
            QCOMPARE(failedCount, 0);
        }

        void page_query_cancel_is_terminal_and_blocks_new_work() {
            auto repository = std::make_shared<SlowCancelableRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int canceledCount = 0;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::canceled, this,
                    [&] { ++canceledCount; });
            ssa::domain::SsaPageRequest firstRequest;
            firstRequest.searchText = "first";

            coordinator.run(firstRequest);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            QElapsedTimer elapsed;
            elapsed.start();
            coordinator.cancel();

            QVERIFY(elapsed.elapsed() < 50);
            QCOMPARE(coordinator.state(),
                     ssa::presentation::PageQueryCoordinator::State::Canceling);
            QCOMPARE(canceledCount, 0);

            auto secondRequest = firstRequest;
            secondRequest.searchText = "second";
            coordinator.run(secondRequest);
            QTest::qWait(50);
            QVERIFY(!repository->secondStarted());
            QCOMPARE(canceledCount, 0);

            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(canceledCount, 1, 1000);
            QCOMPARE(coordinator.state(), ssa::presentation::PageQueryCoordinator::State::Idle);
        }

        void page_query_cancel_does_not_mask_worker_failure() {
            auto repository = std::make_shared<SlowCancelableRepository>(true);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int canceledCount = 0;
            QString failure;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::canceled, this,
                    [&] { ++canceledCount; });
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::failed, this,
                    [&](const QString& message) { failure = message; });
            ssa::domain::SsaPageRequest request;
            request.searchText = "first";

            coordinator.run(request);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);
            QTest::ignoreMessage(QtWarningMsg, "Page query failed: query failed after stop");
            coordinator.cancel();

            QTRY_COMPARE_WITH_TIMEOUT(failure, QString("Falha ao consultar dados"), 1000);
            QVERIFY(!failure.contains("query failed after stop"));
            QCOMPARE(canceledCount, 0);
            QCOMPARE(coordinator.state(), ssa::presentation::PageQueryCoordinator::State::Idle);
        }

        void page_query_terminal_slot_cannot_start_reentrant_work() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int succeededCount = 0;
            connect(
                &coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                [&](const ssa::presentation::PageQueryResult&, const ssa::domain::SsaPageRequest&) {
                    ++succeededCount;
                    if (succeededCount == 1) {
                        ssa::domain::SsaPageRequest secondRequest;
                        secondRequest.searchText = "second";
                        coordinator.run(secondRequest);
                    }
                });

            coordinator.run({});

            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 1, 1000);
            QTest::qWait(50);
            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(coordinator.state(), ssa::presentation::PageQueryCoordinator::State::Idle);
        }

        void column_settings_update_visible_columns_and_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 180));
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(request.visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(request.visibleColumns.front()), QString("numero_ssa"));
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 180,
                                      1000);
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 180);
        }

        void column_settings_allows_wide_description_column() {
            ssa::presentation::ColumnSettingsModel columns;

            QVERIFY(columns.setColumnWidth(QStringLiteral("descricao_ssa"), 900));

            QCOMPARE(columns.maxColumnWidth(), 2400);
            QCOMPARE(columns.columnWidths().at("descricao_ssa"), 900);
        }

        void column_width_update_does_not_reload_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 220);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 220,
                                      1000);
        }

        void apply_column_settings_persists_applied_snapshot_not_later_staging() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 300));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{2},
                                      1000);
            const auto saved = preferences->snapshot();
            QCOMPARE(QString::fromStdString(saved.visibleColumns.at(0)), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(saved.visibleColumns.at(1)), QString("situacao"));
            QCOMPARE(saved.columnWidths.at("numero_ssa"), 220);
        }

        void column_width_flow_persists_without_reloading_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnWidthAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(int, 230));

            QCOMPARE(changed, true);
            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 230);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 230,
                                      1000);
        }

        void immediate_column_width_flow_ignores_unapplied_popup_edits() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 230));
            QVERIFY(model.columns()->setColumnWidth("situacao", 260));

            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnWidthAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(int, 230));

            QCOMPARE(changed, true);
            QCOMPARE(repository->requests().size(), std::size_t{1});
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 230,
                                      1000);
            QCOMPARE(preferences->snapshot().columnWidths.at("situacao"), 160);
            QCOMPARE(preferences->snapshot().visibleColumns.size(), std::size_t{3});
            QCOMPARE(QString::fromStdString(preferences->snapshot().visibleColumns.at(2)),
                     QString("setor_executor"));
        }

        void column_visibility_flow_persists_and_reloads_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "situacao"),
                                      Q_ARG(bool, false));

            QCOMPARE(changed, true);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QCOMPARE(repository->requests().back().visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(repository->requests().back().visibleColumns.front()),
                     QString("numero_ssa"));
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{1},
                                      1000);
        }

        void immediate_column_visibility_flow_ignores_unapplied_popup_edits() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 230));

            bool canHideStagedColumn = false;
            QMetaObject::invokeMethod(model.columnFlow(), "canHideColumn",
                                      qReturnArg(canHideStagedColumn),
                                      Q_ARG(QString, "setor_executor"));
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "situacao"),
                                      Q_ARG(bool, false));

            QCOMPARE(canHideStagedColumn, true);
            QCOMPARE(changed, true);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{2},
                                      1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                QString::fromStdString(preferences->snapshot().visibleColumns.at(0)),
                QString("numero_ssa"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                QString::fromStdString(preferences->snapshot().visibleColumns.at(1)),
                QString("setor_executor"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 140,
                                      1000);
            const auto stagedVisibleKeys = model.columns()->visibleKeys();
            QCOMPARE(stagedVisibleKeys.size(), std::size_t{2});
            QCOMPARE(QString::fromStdString(stagedVisibleKeys.at(0)), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(stagedVisibleKeys.at(1)), QString("setor_executor"));
        }

        void column_visibility_flow_keeps_one_visible_column() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa"};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            bool canHide = true;
            QMetaObject::invokeMethod(model.columnFlow(), "canHideColumn", qReturnArg(canHide),
                                      Q_ARG(QString, "numero_ssa"));
            bool changed = true;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(bool, false));

            QCOMPARE(canHide, false);
            QCOMPARE(changed, false);
            QCOMPARE(model.columns()->visibleKeys().size(), std::size_t{1});
            QCOMPARE(repository->requests().size(), std::size_t{0});
            QCOMPARE(preferences->snapshot().visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(preferences->snapshot().visibleColumns.front()),
                     QString("numero_ssa"));
            QCOMPARE(preferences->saveCount(), 0);
        }

        void theme_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setTheme("tokyo-night");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->theme(), QString("tokyo-night"));
            QCOMPARE(QString::fromStdString(preferences->snapshot().theme), QString("tokyo-night"));
        }

        void sam_refresh_preferences_are_loaded_and_saved() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.samRefresh.enabled = true;
            initial.samRefresh.intervalMinutes = 45;
            initial.samRefresh.baseUrl = "https://apps.example.test/SAM/rest";
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QCOMPARE(model.actions()->workflows()->samRefreshEnabled(), true);
            QCOMPARE(model.actions()->workflows()->samIntervalMinutes(), 45);
            QCOMPARE(model.actions()->workflows()->samBaseUrl(),
                     QStringLiteral("https://apps.example.test/SAM/rest"));

            model.actions()->workflows()->setSamIntervalMinutes(90);

            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().samRefresh.intervalMinutes, 90, 1000);
            QCOMPARE(preferences->snapshot().samRefresh.enabled, true);
        }

        void pyqt_theme_catalog_is_accepted() {
            const QStringList themes{
                "grayscale",
                "windows7",
                "classico",
                "gruvbox",
                "dark",
                "dracula",
                "solarized-dark",
                "solarized-light",
                "mint-light",
                "paper",
                "tokyo-night",
                "catppuccin",
                "nord",
                "ssa-dark",
                "ayu-light",
                "ayu-mirage",
                "flexoki-dark",
                "flexoki-light",
                "kanagawa",
                "kanagawa-dragon",
                "rose-pine",
                "rose-pine-moon",
                "rose-pine-dawn",
                "primer-dark",
                "primer-light",
                "oxocarbon-light",
                "grayscalepy",
                "windows7py",
                "classicopy",
                "gruvboxpy",
                "darkpy",
                "draculapy",
                "solarized-darkpy",
                "solarized-lightpy",
                "mint-lightpy",
                "paperpy",
                "tokyo-nightpy",
                "catppuccinpy",
                "nordpy",
            };

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            for (const QString& theme : themes) {
                model.ui()->setTheme(theme);
                QCOMPARE(model.ui()->theme(), theme);
            }
        }

        void theme_can_be_reverted_after_preview_change() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            const QString original = model.ui()->theme();
            QVERIFY(!original.isEmpty());

            QSignalSpy themeSpy(model.ui(), &ssa::presentation::UiSettingsViewModel::themeChanged);
            model.ui()->setTheme("dark");
            QCOMPARE(model.ui()->theme(), QString("dark"));
            QVERIFY(themeSpy.size() >= 1);

            // Simulate ThemeDialog::reject() restoring the original theme
            themeSpy.clear();
            model.ui()->setTheme(original);
            QCOMPARE(model.ui()->theme(), original);
            QVERIFY(themeSpy.size() >= 1);
        }

        void details_visibility_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDetailsVisible(false);

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->detailsVisible(), false);
            QCOMPARE(preferences->snapshot().detailsVisible, false);
        }

        void details_panel_width_preference_is_saved_and_clamped() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDetailsPanelWidth(620);
            model.ui()->setDetailsPanelWidth(900);

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->detailsPanelWidth(), 900);
            QCOMPARE(preferences->snapshot().detailsPanelWidth, 900);
        }

        void density_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDensity("comfortable");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->density(), QString("comfortable"));
            QCOMPARE(QString::fromStdString(preferences->snapshot().density),
                     QString("comfortable"));
        }

        void saved_filter_apply_matches_name_case_insensitively() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->search()->setText("saved query");
            QVERIFY(QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                              Q_ARG(QString, QString("Mixed Case"))));
            QCOMPARE(model.preferenceFlow()->property("savedFilters").toList().size(), 1);

            model.browse()->search()->setText("changed query");
            QVERIFY(QMetaObject::invokeMethod(model.preferenceFlow(), "applySavedFilter",
                                              Q_ARG(QString, QString("mixed case"))));

            QCOMPARE(model.browse()->search()->text(), QString("saved query"));
        }

        void saved_filter_remove_matches_name_case_insensitively() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->search()->setText("active query");
            QVERIFY(QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                              Q_ARG(QString, QString("Mixed Case"))));
            QCOMPARE(model.preferenceFlow()->property("savedFilters").toList().size(), 1);

            QVERIFY(QMetaObject::invokeMethod(model.preferenceFlow(), "removeSavedFilter",
                                              Q_ARG(QString, QString("MIXED CASE"))));

            QCOMPARE(model.preferenceFlow()->property("savedFilters").toList().size(), 0);
        }

        void current_week_exposes_iso_label_for_header() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QVERIFY(
                model.actions()->currentWeek()->value().contains(QRegularExpression("^\\d{6}$")));
            QVERIFY(model.actions()->currentWeek()->dateTimeLabel().contains(
                QRegularExpression("^\\d{2}/\\d{2}/\\d{4} \\d{2}:\\d{2}$")));
            QVERIFY(!model.actions()->currentWeek()->dateTimeLabel().contains(
                model.actions()->currentWeek()->value()));
        }

        void rescan_incremental_uses_workflow_port_and_updates_status() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->rescanIncremental();

            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->requests().back().mode, ssa::ports::RescanMode::Incremental);
            QCOMPARE(importPort->requests().back().optimized, true);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Reescaneamento concluido"), 1000);
        }

        void import_external_files_uses_workflow_port_and_updates_status() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            selectedFiles.push_back(QUrl::fromLocalFile("/tmp/entrada.xlsx"));
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->importRequests().back().files.size(), std::size_t{1});
            QCOMPARE(importPort->importRequests().back().optimized, true);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Importacao concluida"), 1000);
        }

        void import_success_with_warning_reloads_and_preserves_warning_detail() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>(ssa::ports::WorkflowResult{
                ssa::ports::WorkflowStatus::Succeeded, "consolidation canceled", true});
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);
            QSignalSpy pageSpy(model.browse(), &ssa::presentation::BrowseViewModel::pageChanged);
            QVariantList selectedFiles;
            selectedFiles.push_back(QUrl::fromLocalFile("/tmp/entrada.xlsx"));

            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(repository->countCalls(), std::size_t{1}, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(pageSpy.size() >= 1, 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), true);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Importacao concluida"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->error(),
                                      QString("consolidation canceled"), 1000);
        }

        void import_external_files_rejects_non_local_url() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            selectedFiles.push_back(QUrl("https://example.com/entrada.xlsx"));
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{0}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao importar arquivos"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                model.browse()->status()->error(),
                QString("Falha ao importar arquivos: apenas arquivos locais sao suportados"), 1000);
        }

        void import_external_files_rejects_empty_selection() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{0}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao importar arquivos"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                model.browse()->status()->error(),
                QString("Falha ao importar arquivos: nenhum arquivo selecionado"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void sync_derivadas_updates_status_after_success() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->syncDerivadas();

            QTRY_COMPARE_WITH_TIMEOUT(derivadasPort->syncCalls(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Sincronizacao de derivadas concluida"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), true);
        }

        void compact_database_runs_maintenance_workflow() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto maintenancePort = std::make_shared<CapturingMaintenancePort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, maintenancePort, nullptr);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->compactDatabase();

            QTRY_COMPARE_WITH_TIMEOUT(maintenancePort->vacuumAnalyzeCalls(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Banco compactado"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), true);
        }

        void workflow_success_invalidates_total_rows_all_before_refresh() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);
            QSignalSpy pageSpy(model.browse(), &ssa::presentation::BrowseViewModel::pageChanged);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(repository->countCalls(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(pageSpy.size(), 1, 1000);

            model.actions()->workflows()->syncDerivadas();

            QTRY_COMPARE_WITH_TIMEOUT(repository->countCalls(), std::size_t{2}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Sincronizacao de derivadas concluida"), 1000);
        }

        void sync_derivadas_reports_workflow_error() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>(
                ssa::ports::WorkflowResult{ssa::ports::WorkflowStatus::Failed,
                                           "sync derivadas failed in integration path"});
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->syncDerivadas();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao sincronizar derivadas"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->error(),
                                      QString("sync derivadas failed in integration path"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void sync_derivadas_reports_not_configured_adapter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, nullptr);

            model.actions()->workflows()->syncDerivadas();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao sincronizar derivadas"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->error(),
                                      QString("sync derivadas workflow is not configured"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void rescan_full_disables_optimized_mode() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->rescanFull();

            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->requests().back().mode, ssa::ports::RescanMode::Full);
            QCOMPARE(importPort->requests().back().optimized, false);
        }

        void invalid_density_is_ignored() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDensity("wide");

            QCOMPARE(model.ui()->density(), QString("compact"));
            QCOMPARE(preferences->saveCount(), 0);
        }

        void column_settings_discard_restores_applied_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
            QMetaObject::invokeMethod(model.columnFlow(), "discardColumnSettings");
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QCOMPARE(repository->requests().size(), std::size_t{0});
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 140,
                                      1000);
        }

        void command_view_model_uses_external_command_port() {
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::CommandViewModel model(commands);

            model.openSsa("202500001");

            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            const auto executed = commands->commands().front();
            QCOMPARE(executed.kind, ssa::ports::ExternalCommandKind::OpenSsa);
            QCOMPARE(QString::fromStdString(executed.parameters.at("ssa_number")),
                     QString("202500001"));
            QTRY_COMPARE_WITH_TIMEOUT(model.lastMessage(), QString("ok"), 1000);
            QCOMPARE(model.lastStatus(), QString("succeeded"));
            QCOMPARE(model.lastSucceeded(), true);
        }

        void export_destruction_requests_stop_without_delivering_callback() {
            auto exportPort = std::make_shared<BlockingCancelableExportPort>();
            auto workflows =
                std::make_shared<ssa::application::SsaWorkflowService>(nullptr, exportPort);
            int callbackCount = 0;
            {
                ssa::presentation::ExportViewModel viewModel(
                    workflows, [] { return ssa::domain::SsaPageRequest{}; });
                connect(&viewModel, &ssa::presentation::ExportViewModel::lastResultChanged, this,
                        [&callbackCount] { ++callbackCount; });
                viewModel.exportFilteredList(QUrl::fromLocalFile("/tmp/ssa-export-cancel.csv"));
                QTRY_VERIFY_WITH_TIMEOUT(exportPort->started(), 1000);
            }

            QVERIFY(exportPort->finished());
            QCoreApplication::processEvents();
            QCOMPARE(callbackCount, 0);
        }

        void command_view_model_executes_port_on_owner_thread() {
            auto commands = std::make_shared<ThreadCapturingCommands>();
            ssa::presentation::CommandViewModel model(commands);

            model.openSamHome();

            QTRY_VERIFY_WITH_TIMEOUT(commands->executedThread() != nullptr, 1000);
            QCOMPARE(commands->executedThread(), model.thread());
        }

        void command_view_model_destruction_drops_queued_completion() {
            auto commands = std::make_shared<ThreadCapturingCommands>();
            int resultCallbackCount = 0;
            {
                ssa::presentation::CommandViewModel model(commands);
                connect(&model, &ssa::presentation::CommandViewModel::lastResultChanged, this,
                        [&resultCallbackCount] { ++resultCallbackCount; });
                model.openSamHome();
            }

            QCoreApplication::processEvents();
            QCOMPARE(commands->executedThread(), nullptr);
            QCOMPARE(resultCallbackCount, 0);
        }

        void command_view_model_rejects_wrong_thread_without_mutating_state() {
            auto commands = std::make_shared<ThreadCapturingCommands>();
            ssa::presentation::CommandViewModel model(commands);

            std::thread worker([&model] { model.openSamHome(); });
            worker.join();

            QCOMPARE(commands->executedThread(), nullptr);
            QCOMPARE(model.lastStatus(), QString("idle"));
            QCOMPARE(model.lastMessage(), QString());
        }

        void command_view_model_preserves_not_implemented_status() {
            auto commands = std::make_shared<FakeCommands>();
            commands->nextResult = {ssa::ports::ExternalCommandStatus::NotImplemented,
                                    "not implemented"};
            ssa::presentation::CommandViewModel model(commands);

            model.openSamHome();

            QTRY_COMPARE_WITH_TIMEOUT(model.lastStatus(), QString("not_implemented"), 1000);
            QCOMPARE(model.lastSucceeded(), false);
        }

        void command_view_model_exposes_configured_local_paths() {
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::CommandViewModel model(commands);

            model.openInputFolder();

            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            QCOMPARE(commands->commands().front().kind,
                     ssa::ports::ExternalCommandKind::OpenInputFolder);
            QTRY_COMPARE_WITH_TIMEOUT(model.lastStatus(), QString("succeeded"), 1000);
        }

        void column_move_reorders_visible_keys() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            // Find the first two visible columns to swap.
            const auto keys = model.columns()->visibleKeys();
            QVERIFY(keys.size() >= 2);
            const auto keyA = QString::fromStdString(keys[0]);
            const auto keyB = QString::fromStdString(keys[1]);

            // Find the model rows for these two keys in a single pass.
            int rowA = -1;
            int rowB = -1;
            for (int i = 0; i < model.columns()->rowCount(); ++i) {
                const auto key = model.columns()
                                     ->data(model.columns()->index(i),
                                            ssa::presentation::ColumnSettingsModel::KeyRole)
                                     .toString();
                if (key == keyA) {
                    rowA = i;
                }
                if (key == keyB) {
                    rowB = i;
                }
            }
            QVERIFY(rowA >= 0);
            QVERIFY(rowB >= 0);

            const auto allKeys = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(allKeys[0]), keyA);
            QCOMPARE(QString::fromStdString(allKeys[1]), keyB);

            const bool moved = model.columns()->moveColumn(rowA, rowB);
            QCOMPARE(moved, true);

            const auto keysAfter = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(keysAfter[0]), keyB);
            QCOMPARE(QString::fromStdString(keysAfter[1]), keyA);
        }

        void column_move_persists_order_through_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"situacao", "numero_ssa"};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            const auto initialKeys = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(initialKeys[0]), QString("situacao"));
            QCOMPARE(QString::fromStdString(initialKeys[1]), QString("numero_ssa"));

            // Move column 0 (situacao) to position 1, swapping the visible order.
            int rowA = -1;
            int rowB = -1;
            for (int i = 0; i < model.columns()->rowCount(); ++i) {
                const auto key = model.columns()
                                     ->data(model.columns()->index(i),
                                            ssa::presentation::ColumnSettingsModel::KeyRole)
                                     .toString();
                if (key == QString("situacao")) {
                    rowA = i;
                }
                if (key == QString("numero_ssa")) {
                    rowB = i;
                }
            }
            QVERIFY(rowA >= 0);
            QVERIFY(rowB >= 0);

            const bool moved = model.columns()->moveColumn(rowA, rowB);
            QCOMPARE(moved, true);
            qobject_cast<ssa::presentation::MainColumnFlowCoordinator*>(model.columnFlow())
                ->applyColumnSettings();

            // Verify the in-memory order flipped.
            const auto reordered = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(reordered[0]), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(reordered[1]), QString("situacao"));

            // Verify the new order actually persisted into the preferences store.
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.front(),
                                      std::string("numero_ssa"), 1000);
            QCOMPARE(preferences->snapshot().visibleColumns.at(1), std::string("situacao"));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationSmokeTest)

#include "PresentationSmokeTest.moc"
