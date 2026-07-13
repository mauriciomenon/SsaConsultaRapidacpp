#pragma once

#include "domain/SsaTypes.h"
#include "ports/IExternalCommandPort.h"
#include "ports/IFilterPresetStore.h"
#include "ports/ISsaRepository.h"
#include "ports/IUserPreferencesStore.h"
#include "ports/IWorkflowPorts.h"
#include "presentation/FilterPanelViewModel.h"

#include <QChar>
#include <QString>
#include <QVariantMap>

#include <chrono>
#include <exception>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ssa::tests::presentation_smoke {

    struct FakeRepositoryConfig final {
        std::chrono::milliseconds delay{0};
        std::size_t totalRows{1};
        std::size_t rowCount{1};
    };

    class FakeRepository final : public ssa::ports::ISsaRepository {
      public:
        explicit FakeRepository(FakeRepositoryConfig config = {})
            : delay_(config.delay), totalRows_(config.totalRows), rowCount_(config.rowCount) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token = {}) const override {
            {
                const std::scoped_lock lock(mutex_);
                startedRequests_.push_back(request);
            }
            if (delay_.count() > 0) {
                std::this_thread::sleep_for(delay_);
            }
            {
                const std::scoped_lock lock(mutex_);
                requests_.push_back(request);
            }
            std::vector<ssa::domain::SsaRecord> rows;
            rows.reserve(rowCount_);
            for (std::size_t i = 0; i < rowCount_; ++i) {
                const auto ssaNumber =
                    QString("2025%1").arg(i + 1, 5, 10, QChar('0')).toStdString();
                rows.push_back(ssa::domain::SsaRecord{
                    {{"numero_ssa", ssaNumber},
                     {"situacao", "APV"},
                     {"descricao_ssa",
                      request.searchText.empty() ? "Inicial" : request.searchText}}});
            }
            return {rows, totalRows_, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token = {}) const override {
            const std::scoped_lock lock(mutex_);
            ++countCalls_;
            return totalRows_;
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

        [[nodiscard]] std::size_t maxValueLength(std::string_view,
                                                 std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest& request,
                                          ssa::ports::SsaRecordConsumer consume,
                                          std::stop_token = {}) const override {
            auto pageResult = page(request);
            std::size_t rowCount = 0;
            for (const auto& row : pageResult.rows) {
                if (auto error = consume(row); error.has_value()) {
                    return {rowCount, *error};
                }
                ++rowCount;
            }
            return {rowCount, {}};
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> startedRequests() const {
            const std::scoped_lock lock(mutex_);
            return startedRequests_;
        }

        [[nodiscard]] std::size_t countCalls() const {
            const std::scoped_lock lock(mutex_);
            return countCalls_;
        }

      private:
        std::chrono::milliseconds delay_;
        std::size_t totalRows_;
        std::size_t rowCount_;
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::SsaPageRequest> startedRequests_;
        mutable std::vector<ssa::domain::SsaPageRequest> requests_;
        mutable std::size_t countCalls_{0};
    };

    class FakeCommands final : public ssa::ports::IExternalCommandPort {
      public:
        ssa::ports::ExternalCommandResult
        execute(const ssa::ports::ExternalCommand& command) override {
            const std::scoped_lock lock(mutex);
            commands_.push_back(command);
            return nextResult;
        }

        [[nodiscard]] std::vector<ssa::ports::ExternalCommand> commands() const {
            const std::scoped_lock lock(mutex);
            return commands_;
        }

        ssa::ports::ExternalCommandResult nextResult{ssa::ports::ExternalCommandStatus::Succeeded,
                                                     "ok"};

      private:
        mutable std::mutex mutex;
        std::vector<ssa::ports::ExternalCommand> commands_;
    };

    [[nodiscard]] QVariantMap
    activeFilterEntry(const ssa::presentation::FilterPanelViewModel* filters, const QString& kind,
                      const QString& key = {}) {
        for (const auto& entry : filters->activeFilterEntries()) {
            const auto map = entry.toMap();
            if (map.value(QStringLiteral("kind")).toString() != kind) {
                continue;
            }
            if (!key.isEmpty() && map.value(QStringLiteral("key")).toString() != key) {
                continue;
            }
            return map;
        }
        return {};
    }

    class FakePreferences final : public ssa::ports::IUserPreferencesStore {
      public:
        class SaveFailure final : public std::exception {
          public:
            explicit SaveFailure(std::string message) : message_(std::move(message)) {}

            [[nodiscard]] const char* what() const noexcept override {
                return message_.c_str();
            }

          private:
            std::string message_;
        };

        explicit FakePreferences(ssa::ports::UserPreferencesSnapshot initial = {})
            : snapshot_(std::move(initial)) {}

        ssa::ports::UserPreferencesSnapshot load() const override {
            const std::scoped_lock lock(mutex_);
            return snapshot_;
        }

        void save(const ssa::ports::UserPreferencesSnapshot& snapshot) const override {
            const std::scoped_lock lock(mutex_);
            if (!saveError_.empty()) {
                auto saveError = saveError_;
                saveError_.clear();
                throw SaveFailure(std::move(saveError));
            }
            snapshot_ = snapshot;
            ++saveCount_;
        }

        void failNextSave(std::string message) const {
            const std::scoped_lock lock(mutex_);
            saveError_ = std::move(message);
        }

        [[nodiscard]] ssa::ports::UserPreferencesSnapshot snapshot() const {
            const std::scoped_lock lock(mutex_);
            return snapshot_;
        }

        [[nodiscard]] int saveCount() const {
            const std::scoped_lock lock(mutex_);
            return saveCount_;
        }

      private:
        mutable ssa::ports::UserPreferencesSnapshot snapshot_;
        mutable int saveCount_{0};
        mutable std::string saveError_;
        mutable std::mutex mutex_;
    };

    class FakeFilterPresetStore final : public ssa::ports::IFilterPresetStore {
      public:
        ssa::ports::FilterPresetSnapshot load(std::filesystem::path) const override {
            const std::scoped_lock lock(mutex_);
            return nextLoad_;
        }

        void save(std::filesystem::path path,
                  const ssa::ports::FilterPresetSnapshot& snapshot) const override {
            const std::scoped_lock lock(mutex_);
            savedPath_ = std::move(path);
            saved_ = snapshot;
            ++saveCount_;
        }

        void setNextLoad(ssa::ports::FilterPresetSnapshot snapshot) {
            const std::scoped_lock lock(mutex_);
            nextLoad_ = std::move(snapshot);
        }

        [[nodiscard]] int saveCount() const {
            const std::scoped_lock lock(mutex_);
            return saveCount_;
        }

        [[nodiscard]] ssa::ports::FilterPresetSnapshot saved() const {
            const std::scoped_lock lock(mutex_);
            return saved_;
        }

      private:
        mutable std::mutex mutex_;
        mutable ssa::ports::FilterPresetSnapshot saved_;
        mutable std::filesystem::path savedPath_;
        mutable int saveCount_{0};
        ssa::ports::FilterPresetSnapshot nextLoad_;
    };

    class CapturingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        explicit CapturingImportPort(ssa::ports::WorkflowResult result =
                                         {ssa::ports::WorkflowStatus::Succeeded, "import staged"})
            : nextResult_(std::move(result)) {}

        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest& request,
                            std::stop_token = {}) override {
            const std::scoped_lock lock(mutex_);
            importRequests_.push_back(request);
            return nextResult_;
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest& request,
                                          std::stop_token = {}) override {
            const std::scoped_lock lock(mutex_);
            requests_.push_back(request);
            return nextResult_;
        }

        [[nodiscard]] std::vector<ssa::ports::RescanRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

        [[nodiscard]] std::vector<ssa::ports::ImportExternalFilesRequest> importRequests() const {
            const std::scoped_lock lock(mutex_);
            return importRequests_;
        }

      private:
        mutable std::mutex mutex_;
        std::vector<ssa::ports::ImportExternalFilesRequest> importRequests_;
        std::vector<ssa::ports::RescanRequest> requests_;
        ssa::ports::WorkflowResult nextResult_;
    };

    class CapturingDerivadasPort final : public ssa::ports::IDerivadasPort {
      public:
        explicit CapturingDerivadasPort(
            ssa::ports::WorkflowResult result = {ssa::ports::WorkflowStatus::Succeeded,
                                                 "derivadas sync completed"})
            : nextResult_(std::move(result)) {}

        ssa::ports::WorkflowResult syncDerivadas(std::stop_token = {}) override {
            const std::scoped_lock lock(mutex_);
            ++syncCalls_;
            return nextResult_;
        }

        [[nodiscard]] std::size_t syncCalls() const {
            const std::scoped_lock lock(mutex_);
            return syncCalls_;
        }

        void setNextResult(ssa::ports::WorkflowResult result) {
            const std::scoped_lock lock(mutex_);
            nextResult_ = std::move(result);
        }

      private:
        mutable std::mutex mutex_;
        std::size_t syncCalls_{0};
        ssa::ports::WorkflowResult nextResult_;
    };

    class CapturingMaintenancePort final : public ssa::ports::IDatabaseMaintenancePort {
      public:
        ssa::ports::WorkflowResult resetDatabase(std::stop_token = {}) override {
            return {ssa::ports::WorkflowStatus::Succeeded, "reset database requested"};
        }

        ssa::ports::WorkflowResult cleanData(std::stop_token = {}) override {
            return {ssa::ports::WorkflowStatus::Succeeded, "clean data requested"};
        }

        ssa::ports::WorkflowResult vacuumAnalyze(std::stop_token = {}) override {
            const std::scoped_lock lock(mutex_);
            ++vacuumAnalyzeCalls_;
            return nextResult_;
        }

        [[nodiscard]] std::size_t vacuumAnalyzeCalls() const {
            const std::scoped_lock lock(mutex_);
            return vacuumAnalyzeCalls_;
        }

        void setNextResult(ssa::ports::WorkflowResult result) {
            const std::scoped_lock lock(mutex_);
            nextResult_ = std::move(result);
        }

      private:
        mutable std::mutex mutex_;
        std::size_t vacuumAnalyzeCalls_{0};
        ssa::ports::WorkflowResult nextResult_{ssa::ports::WorkflowStatus::Succeeded,
                                               "database compacted"};
    };

} // namespace ssa::tests::presentation_smoke
