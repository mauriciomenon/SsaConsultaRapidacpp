#include "platform/SupervisedProcess.h"

#include <QObject>
#include <QProcess>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <utility>

#ifdef Q_OS_WIN
#include <vector>
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace ssa::platform {
    namespace {

        using namespace std::chrono_literals;

        constexpr qsizetype kDiagnosticLimit = 4'096;
        constexpr auto kPollInterval = 25ms;
        constexpr auto kGracePeriod = 500ms;
        constexpr auto kForcedStopTimeout = 5s;

        std::mutex processRegistryMutex;
        std::atomic_bool forceStopRequested{false};
        std::atomic_bool forceStopFailed{false};
        std::atomic_bool untrackedStopFailure{false};
#ifdef SSA_SUPERVISED_PROCESS_TESTING
        std::atomic_bool stopFailureForTesting{false};
        std::atomic_int postStartPauseMsForTesting{0};
#endif
        std::size_t processStartsInFlight = 0;

        void recordStopFailureTracking(const bool tracked) {
            if (!tracked) {
                const std::scoped_lock lock(processRegistryMutex);
                untrackedStopFailure.store(true, std::memory_order_relaxed);
                forceStopRequested.store(true, std::memory_order_relaxed);
                forceStopFailed.store(true, std::memory_order_relaxed);
            }
        }

#ifdef Q_OS_WIN
        struct RegisteredTreeEntry final {
            ~RegisteredTreeEntry() {
                if (job != nullptr) {
                    CloseHandle(job);
                }
            }

            HANDLE job = nullptr;
            std::optional<std::chrono::steady_clock::time_point> emptySince;
        };

        std::vector<std::shared_ptr<RegisteredTreeEntry>> processRegistry;

        class RegisteredProcessTree final {
          public:
            explicit RegisteredProcessTree(const HANDLE job) {
                HANDLE duplicate = nullptr;
                if (job == nullptr ||
                    DuplicateHandle(GetCurrentProcess(), job, GetCurrentProcess(), &duplicate, 0,
                                    FALSE, DUPLICATE_SAME_ACCESS) == 0) {
                    return;
                }
                entry_ = std::make_shared<RegisteredTreeEntry>();
                entry_->job = duplicate;
                const std::scoped_lock lock(processRegistryMutex);
                processRegistry.push_back(entry_);
            }

            ~RegisteredProcessTree() {
                if (!entry_ || retained_) {
                    return;
                }
                const std::scoped_lock lock(processRegistryMutex);
                std::erase(processRegistry, entry_);
            }

            RegisteredProcessTree(const RegisteredProcessTree&) = delete;
            RegisteredProcessTree& operator=(const RegisteredProcessTree&) = delete;

            [[nodiscard]] bool valid() const {
                return entry_ != nullptr;
            }

            void retain() {
                retained_ = true;
            }

          private:
            std::shared_ptr<RegisteredTreeEntry> entry_;
            bool retained_ = false;
        };

        bool retainFailedProcessTree(const HANDLE job) {
            RegisteredProcessTree failedTree{job};
            if (!failedTree.valid()) {
                return false;
            }
            failedTree.retain();
            return true;
        }
#else
        struct RegisteredTreeEntry final {
            qint64 processGroup = 0;
        };

        std::set<std::shared_ptr<RegisteredTreeEntry>> processRegistry;

        class RegisteredProcessTree final {
          public:
            explicit RegisteredProcessTree(const qint64 processGroup) {
                if (processGroup <= 0) {
                    return;
                }
                entry_ = std::make_shared<RegisteredTreeEntry>();
                entry_->processGroup = processGroup;
                const std::scoped_lock lock(processRegistryMutex);
                processRegistry.insert(entry_);
            }

            ~RegisteredProcessTree() {
                if (!entry_ || retained_) {
                    return;
                }
                const std::scoped_lock lock(processRegistryMutex);
                processRegistry.erase(entry_);
            }

            RegisteredProcessTree(const RegisteredProcessTree&) = delete;
            RegisteredProcessTree& operator=(const RegisteredProcessTree&) = delete;

            [[nodiscard]] bool valid() const {
                return entry_ != nullptr;
            }

            void retain() {
                retained_ = true;
            }

          private:
            std::shared_ptr<RegisteredTreeEntry> entry_;
            bool retained_ = false;
        };

        bool retainFailedProcessTree(const qint64 processGroup) {
            RegisteredProcessTree failedTree{processGroup};
            if (!failedTree.valid()) {
                return false;
            }
            failedTree.retain();
            return true;
        }
#endif

        class ProcessStartRegistration final {
          public:
            ProcessStartRegistration() {
                const std::scoped_lock lock(processRegistryMutex);
                if (!forceStopRequested.load(std::memory_order_relaxed) &&
                    !forceStopFailed.load(std::memory_order_relaxed)) {
                    ++processStartsInFlight;
                    active_ = true;
                }
            }

            ~ProcessStartRegistration() {
                complete();
            }

            ProcessStartRegistration(const ProcessStartRegistration&) = delete;
            ProcessStartRegistration& operator=(const ProcessStartRegistration&) = delete;

            [[nodiscard]] bool allowed() const {
                return active_;
            }

            void complete() {
                if (!active_) {
                    return;
                }
                const std::scoped_lock lock(processRegistryMutex);
                --processStartsInFlight;
                active_ = false;
            }

          private:
            bool active_{false};
        };

        void appendLimited(QByteArray& destination, const QByteArray& source) {
            const auto remaining = kDiagnosticLimit - destination.size();
            if (remaining > 0) {
                destination.append(source.first((std::min)(remaining, source.size())));
            }
        }

        void drainProcess(QProcess& process, QByteArray& standardError,
                          QByteArray& standardOutput) {
            appendLimited(standardError, process.readAllStandardError());
            appendLimited(standardOutput, process.readAllStandardOutput());
        }

        void stopProcessLeader(QProcess& process) {
            if (process.state() == QProcess::NotRunning) {
                return;
            }
            process.kill();
            process.waitForFinished(static_cast<int>(kForcedStopTimeout.count()));
        }

        QString diagnosticText(QProcess& process, const QByteArray& standardError,
                               const QByteArray& standardOutput) {
            if (!standardError.isEmpty()) {
                return QString::fromLocal8Bit(standardError).trimmed();
            }
            if (!standardOutput.isEmpty()) {
                return QString::fromLocal8Bit(standardOutput).trimmed();
            }
            return process.errorString().trimmed();
        }

#ifdef Q_OS_WIN
        std::optional<bool> jobActive(const HANDLE job) {
            if (job == nullptr) {
                return false;
            }
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION information{};
            if (QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &information,
                                          sizeof(information), nullptr) == 0) {
                return std::nullopt;
            }
            return information.ActiveProcesses > 0;
        }

        class JobHandle final {
          public:
            JobHandle() : handle_(CreateJobObjectW(nullptr, nullptr)) {
                if (handle_ == nullptr) {
                    return;
                }
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION information{};
                information.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                if (SetInformationJobObject(handle_, JobObjectExtendedLimitInformation,
                                            &information, sizeof(information)) == 0) {
                    CloseHandle(handle_);
                    handle_ = nullptr;
                }
            }

            ~JobHandle() {
                if (handle_ != nullptr) {
                    CloseHandle(handle_);
                }
            }

            JobHandle(const JobHandle&) = delete;
            JobHandle& operator=(const JobHandle&) = delete;

            [[nodiscard]] HANDLE get() const {
                return handle_;
            }

            [[nodiscard]] bool valid() const {
                return handle_ != nullptr;
            }

            [[nodiscard]] bool active() const {
                return jobActive(handle_).value_or(true);
            }

            [[nodiscard]] bool terminate() const {
                return handle_ != nullptr && TerminateJobObject(handle_, 1) != 0;
            }

          private:
            HANDLE handle_ = nullptr;
        };

        struct WindowsStartup final {
            explicit WindowsStartup(const HANDLE job) : jobHandle(job) {
                SIZE_T bytes = 0;
                InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
                storage.resize(bytes);
                startup.StartupInfo.cb = sizeof(startup);
                startup.lpAttributeList =
                    reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
                initialized =
                    InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &bytes) != 0;
                valid = initialized &&
                        UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                                                  PROC_THREAD_ATTRIBUTE_JOB_LIST,
                                                  static_cast<void*>(&jobHandle), sizeof(jobHandle),
                                                  nullptr, nullptr) != 0;
            }

            ~WindowsStartup() {
                if (initialized) {
                    DeleteProcThreadAttributeList(startup.lpAttributeList);
                }
            }

            STARTUPINFOEXW startup{};
            std::vector<std::byte> storage;
            HANDLE jobHandle = nullptr;
            bool initialized = false;
            bool valid = false;
        };

        bool treeActive(const JobHandle& job, const qint64) {
            return job.active();
        }

        bool terminateTree(QProcess& process, const JobHandle& job, const qint64) {
#ifdef SSA_SUPERVISED_PROCESS_TESTING
            if (stopFailureForTesting.load(std::memory_order_relaxed)) {
                return false;
            }
#endif
            if (process.state() == QProcess::NotRunning && !job.active()) {
                return true;
            }
            process.terminate();
            const auto gracefulDeadline = std::chrono::steady_clock::now() + kGracePeriod;
            while (job.active() && std::chrono::steady_clock::now() < gracefulDeadline) {
                process.waitForFinished(static_cast<int>(kPollInterval.count()));
            }
            if (job.active() && !job.terminate()) {
                return false;
            }
            if (process.state() != QProcess::NotRunning) {
                process.kill();
            }
            const auto forcedDeadline = std::chrono::steady_clock::now() + kForcedStopTimeout;
            while (job.active() && std::chrono::steady_clock::now() < forcedDeadline) {
                process.waitForFinished(static_cast<int>(kPollInterval.count()));
            }
            process.waitForFinished(static_cast<int>(kPollInterval.count()));
            return !job.active() && process.state() == QProcess::NotRunning;
        }
#else
        bool treeActive(const qint64 processGroup) {
            if (processGroup <= 0) {
                return false;
            }
            errno = 0;
            return ::kill(-static_cast<pid_t>(processGroup), 0) == 0 || errno == EPERM;
        }

        bool terminateTree(QProcess& process, const qint64 processGroup) {
#ifdef SSA_SUPERVISED_PROCESS_TESTING
            if (stopFailureForTesting.load(std::memory_order_relaxed)) {
                return false;
            }
#endif
            if (processGroup <= 0) {
                if (process.state() == QProcess::NotRunning) {
                    return true;
                }
                process.kill();
                process.waitForFinished(static_cast<int>(kForcedStopTimeout.count()));
                return process.state() == QProcess::NotRunning;
            }
            if (treeActive(processGroup)) {
                ::kill(-static_cast<pid_t>(processGroup), SIGTERM);
            } else if (process.state() != QProcess::NotRunning) {
                process.terminate();
            }
            const auto gracefulDeadline = std::chrono::steady_clock::now() + kGracePeriod;
            while (treeActive(processGroup) &&
                   std::chrono::steady_clock::now() < gracefulDeadline) {
                process.waitForFinished(static_cast<int>(kPollInterval.count()));
            }
            if (treeActive(processGroup)) {
                ::kill(-static_cast<pid_t>(processGroup), SIGKILL);
            }
            if (process.state() != QProcess::NotRunning) {
                process.kill();
            }
            const auto forcedDeadline = std::chrono::steady_clock::now() + kForcedStopTimeout;
            while (treeActive(processGroup) && std::chrono::steady_clock::now() < forcedDeadline) {
                process.waitForFinished(static_cast<int>(kPollInterval.count()));
            }
            process.waitForFinished(static_cast<int>(kPollInterval.count()));
            return !treeActive(processGroup) && process.state() == QProcess::NotRunning;
        }
#endif

    } // namespace

    SupervisedProcessResult SupervisedProcess::run(const SupervisedProcessRequest& request,
                                                   const std::stop_token& stopToken) {
        if (stopToken.stop_requested()) {
            return {SupervisedProcessStatus::Canceled, -1, {}};
        }
        if (request.program.isEmpty() || request.timeout <= 0ms) {
            return {SupervisedProcessStatus::StartFailed, -1,
                    QStringLiteral("invalid supervised process request")};
        }

        ProcessStartRegistration startRegistration;
        if (!startRegistration.allowed()) {
            return {SupervisedProcessStatus::Canceled, -1, {}};
        }

        QProcess process;
        process.setProgram(request.program);
        process.setArguments(request.arguments);
        if (!request.workingDirectory.isEmpty()) {
            process.setWorkingDirectory(request.workingDirectory);
        }
#ifdef Q_OS_WIN
        JobHandle job;
        if (!job.valid()) {
            return {SupervisedProcessStatus::StartFailed, -1,
                    QStringLiteral("cannot create process job")};
        }
        WindowsStartup startup{job.get()};
        if (!startup.valid) {
            return {SupervisedProcessStatus::StartFailed, -1,
                    QStringLiteral("cannot configure process job")};
        }
        process.setCreateProcessArgumentsModifier(
            [&startup](QProcess::CreateProcessArguments* args) {
                startup.startup.StartupInfo = *args->startupInfo;
                startup.startup.StartupInfo.cb = sizeof(startup.startup);
                args->startupInfo = reinterpret_cast<Q_STARTUPINFO*>(&startup.startup);
                args->flags |= EXTENDED_STARTUPINFO_PRESENT;
            });
#else
        process.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
#endif

        QByteArray standardError;
        QByteArray standardOutput;
        const auto deadline = std::chrono::steady_clock::now() + request.timeout;
        qint64 processGroup = 0;
        std::optional<RegisteredProcessTree> registeredTree;
        QObject connectionContext;
        QObject::connect(&process, &QProcess::started, &connectionContext, [&] {
            processGroup = process.processId();
            if (registeredTree.has_value()) {
                return;
            }
#ifdef Q_OS_WIN
            registeredTree.emplace(job.get());
#else
            registeredTree.emplace(processGroup);
#endif
        });
        process.start();
        while (process.state() == QProcess::Starting) {
            const bool forced = forceStopRequested.load(std::memory_order_relaxed);
            if (stopToken.stop_requested() || forced) {
                process.waitForStarted(static_cast<int>(kPollInterval.count()));
                const auto startingProcessGroup =
                    processGroup > 0 ? processGroup : process.processId();
#ifdef Q_OS_WIN
                const auto stopped = terminateTree(process, job, startingProcessGroup);
#else
                const auto stopped = terminateTree(process, startingProcessGroup);
#endif
                if (!stopped) {
#ifdef Q_OS_WIN
                    const auto tracked = retainFailedProcessTree(job.get());
#else
                    const auto tracked = retainFailedProcessTree(startingProcessGroup);
#endif
                    recordStopFailureTracking(tracked);
                    stopProcessLeader(process);
                }
                return {stopped ? SupervisedProcessStatus::Canceled
                                : SupervisedProcessStatus::FailedToStop,
                        process.exitCode(), diagnosticText(process, standardError, standardOutput)};
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                process.waitForStarted(static_cast<int>(kPollInterval.count()));
                const auto startingProcessGroup =
                    processGroup > 0 ? processGroup : process.processId();
#ifdef Q_OS_WIN
                const auto stopped = terminateTree(process, job, startingProcessGroup);
#else
                const auto stopped = terminateTree(process, startingProcessGroup);
#endif
                if (!stopped) {
#ifdef Q_OS_WIN
                    const auto tracked = retainFailedProcessTree(job.get());
#else
                    const auto tracked = retainFailedProcessTree(startingProcessGroup);
#endif
                    recordStopFailureTracking(tracked);
                    stopProcessLeader(process);
                }
                return {stopped ? SupervisedProcessStatus::TimedOut
                                : SupervisedProcessStatus::FailedToStop,
                        process.exitCode(), diagnosticText(process, standardError, standardOutput)};
            }
            process.waitForStarted(static_cast<int>(kPollInterval.count()));
            drainProcess(process, standardError, standardOutput);
        }
#ifdef SSA_SUPERVISED_PROCESS_TESTING
        const auto postStartPause = postStartPauseMsForTesting.load(std::memory_order_relaxed);
        if (postStartPause > 0) {
            QThread::msleep(static_cast<unsigned long>(postStartPause));
        }
#endif
        if (!registeredTree.has_value()) {
            drainProcess(process, standardError, standardOutput);
            return {SupervisedProcessStatus::StartFailed, process.exitCode(),
                    diagnosticText(process, standardError, standardOutput)};
        }
        if (!registeredTree->valid()) {
#ifdef Q_OS_WIN
            const auto stopped = terminateTree(process, job, processGroup);
#else
            const auto stopped = terminateTree(process, processGroup);
#endif
            if (!stopped) {
#ifdef Q_OS_WIN
                const auto tracked = retainFailedProcessTree(job.get());
#else
                const auto tracked = retainFailedProcessTree(processGroup);
#endif
                recordStopFailureTracking(tracked);
                stopProcessLeader(process);
            }
            return {stopped ? SupervisedProcessStatus::StartFailed
                            : SupervisedProcessStatus::FailedToStop,
                    process.exitCode(), QStringLiteral("cannot register process tree")};
        }
        startRegistration.complete();

        while (process.state() != QProcess::NotRunning) {
            drainProcess(process, standardError, standardOutput);
            const bool forced = forceStopRequested.load(std::memory_order_relaxed);
            if (stopToken.stop_requested() || forced) {
#ifdef Q_OS_WIN
                const auto stopped = terminateTree(process, job, processGroup);
#else
                const auto stopped = terminateTree(process, processGroup);
#endif
                if (!stopped) {
                    registeredTree->retain();
                    stopProcessLeader(process);
                }
                drainProcess(process, standardError, standardOutput);
                return {stopped ? SupervisedProcessStatus::Canceled
                                : SupervisedProcessStatus::FailedToStop,
                        process.exitCode(), diagnosticText(process, standardError, standardOutput)};
            }
            if (std::chrono::steady_clock::now() >= deadline) {
#ifdef Q_OS_WIN
                const auto stopped = terminateTree(process, job, processGroup);
#else
                const auto stopped = terminateTree(process, processGroup);
#endif
                if (!stopped) {
                    registeredTree->retain();
                    stopProcessLeader(process);
                }
                drainProcess(process, standardError, standardOutput);
                return {stopped ? SupervisedProcessStatus::TimedOut
                                : SupervisedProcessStatus::FailedToStop,
                        process.exitCode(), diagnosticText(process, standardError, standardOutput)};
            }
            process.waitForFinished(static_cast<int>(kPollInterval.count()));
        }
        drainProcess(process, standardError, standardOutput);

        if (forceStopRequested.load(std::memory_order_relaxed)) {
#ifdef Q_OS_WIN
            const auto stopped = terminateTree(process, job, processGroup);
#else
            const auto stopped = terminateTree(process, processGroup);
#endif
            if (!stopped) {
                registeredTree->retain();
            }
            return {stopped ? SupervisedProcessStatus::Canceled
                            : SupervisedProcessStatus::FailedToStop,
                    process.exitCode(), diagnosticText(process, standardError, standardOutput)};
        }

#ifdef Q_OS_WIN
        auto residualTree = treeActive(job, processGroup);
        const auto accountingDeadline = std::chrono::steady_clock::now() + kGracePeriod;
        while (residualTree && std::chrono::steady_clock::now() < accountingDeadline) {
            QThread::msleep(static_cast<unsigned long>(kPollInterval.count()));
            residualTree = treeActive(job, processGroup);
        }
        const auto stoppedResidual = !residualTree || terminateTree(process, job, processGroup);
#else
        const auto residualTree = treeActive(processGroup);
        const auto stoppedResidual = !residualTree || terminateTree(process, processGroup);
#endif
        if (!stoppedResidual) {
            registeredTree->retain();
            return {SupervisedProcessStatus::FailedToStop, process.exitCode(),
                    diagnosticText(process, standardError, standardOutput)};
        }
        if (residualTree) {
            return {SupervisedProcessStatus::Failed, process.exitCode(),
                    QStringLiteral("process leader exited with active descendants")};
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            return {SupervisedProcessStatus::Failed, process.exitCode(),
                    diagnosticText(process, standardError, standardOutput)};
        }
        return {SupervisedProcessStatus::Succeeded, process.exitCode(), {}};
    }

    ForceStopRequestStatus SupervisedProcess::requestForceStopAll() {
        const std::scoped_lock lock(processRegistryMutex);
        forceStopRequested.store(true, std::memory_order_relaxed);
        bool stopped = true;
#ifdef Q_OS_WIN
        const auto observationTime = std::chrono::steady_clock::now();
        for (auto entry = processRegistry.begin(); entry != processRegistry.end();) {
#ifdef SSA_SUPERVISED_PROCESS_TESTING
            const auto signaled = !stopFailureForTesting.load(std::memory_order_relaxed) &&
                                  TerminateJobObject((*entry)->job, 1) != 0;
#else
            const auto signaled = TerminateJobObject((*entry)->job, 1) != 0;
#endif
            stopped = signaled && stopped;
            const auto active = jobActive((*entry)->job);
            if (!active) {
                stopped = false;
                (*entry)->emptySince.reset();
                ++entry;
            } else if (*active) {
                (*entry)->emptySince.reset();
                ++entry;
            } else if (!(*entry)->emptySince) {
                (*entry)->emptySince = observationTime;
                ++entry;
            } else if (observationTime - *(*entry)->emptySince < kPollInterval) {
                ++entry;
            } else {
                entry = processRegistry.erase(entry);
            }
        }
#else
        for (auto entry = processRegistry.begin(); entry != processRegistry.end();) {
            const auto processGroup = (*entry)->processGroup;
#ifdef SSA_SUPERVISED_PROCESS_TESTING
            const auto injectedFailure = stopFailureForTesting.load(std::memory_order_relaxed);
#else
            constexpr bool injectedFailure = false;
#endif
            if (injectedFailure ||
                (::kill(-static_cast<pid_t>(processGroup), SIGKILL) != 0 && errno != ESRCH)) {
                stopped = false;
            }
            if (treeActive(processGroup)) {
                ++entry;
            } else {
                entry = processRegistry.erase(entry);
            }
        }
#endif
        if (!stopped) {
            forceStopFailed.store(true, std::memory_order_relaxed);
        }
        const bool drained = processStartsInFlight == 0 && processRegistry.empty() &&
                             !untrackedStopFailure.load(std::memory_order_relaxed);
        if (drained) {
            return ForceStopRequestStatus::Drained;
        }
        if (forceStopFailed.load(std::memory_order_relaxed)) {
            return ForceStopRequestStatus::Failed;
        }
        return ForceStopRequestStatus::Pending;
    }

    bool SupervisedProcess::forceStopAll() {
        const auto deadline = std::chrono::steady_clock::now() + kForcedStopTimeout;
        while (true) {
            if (requestForceStopAll() == ForceStopRequestStatus::Drained) {
                const std::scoped_lock lock(processRegistryMutex);
                if (processStartsInFlight != 0 || !processRegistry.empty()) {
                    continue;
                }
                forceStopRequested.store(false, std::memory_order_relaxed);
                forceStopFailed.store(false, std::memory_order_relaxed);
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            QThread::msleep(static_cast<unsigned long>(kPollInterval.count()));
        }
    }

#ifdef SSA_SUPERVISED_PROCESS_TESTING
    namespace supervised_process_testing {
        void setStopFailure(const bool enabled) {
            stopFailureForTesting.store(enabled, std::memory_order_relaxed);
        }

        void setPostStartPause(const std::chrono::milliseconds duration) {
            postStartPauseMsForTesting.store(static_cast<int>(duration.count()),
                                             std::memory_order_relaxed);
        }

        std::size_t registeredTreeCount() {
            const std::scoped_lock lock(processRegistryMutex);
            return processRegistry.size();
        }

        void setUntrackedStopFailure(const bool enabled) {
            const std::scoped_lock lock(processRegistryMutex);
            untrackedStopFailure.store(enabled, std::memory_order_relaxed);
            if (enabled) {
                forceStopRequested.store(true, std::memory_order_relaxed);
                forceStopFailed.store(true, std::memory_order_relaxed);
            }
        }

        void recordTrackedStopFailure() {
            recordStopFailureTracking(true);
        }

        void recordUntrackableStopFailure() {
#ifdef Q_OS_WIN
            recordStopFailureTracking(retainFailedProcessTree(nullptr));
#else
            recordStopFailureTracking(retainFailedProcessTree(0));
#endif
        }
    } // namespace supervised_process_testing
#endif

} // namespace ssa::platform
