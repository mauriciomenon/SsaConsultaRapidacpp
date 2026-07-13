#include "platform/SupervisedProcess.h"

#include <QProcess>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <set>
#include <utility>

#ifdef Q_OS_WIN
#include <memory>
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
        std::size_t processStartsInFlight = 0;

#ifdef Q_OS_WIN
        std::vector<HANDLE> processRegistry;

        class RegisteredProcessTree final {
          public:
            explicit RegisteredProcessTree(const HANDLE job) : job_(job) {
                const std::scoped_lock lock(processRegistryMutex);
                processRegistry.push_back(job_);
            }

            ~RegisteredProcessTree() {
                const std::scoped_lock lock(processRegistryMutex);
                std::erase(processRegistry, job_);
            }

            RegisteredProcessTree(const RegisteredProcessTree&) = delete;
            RegisteredProcessTree& operator=(const RegisteredProcessTree&) = delete;

          private:
            HANDLE job_ = nullptr;
        };
#else
        std::set<qint64> processRegistry;

        class RegisteredProcessTree final {
          public:
            explicit RegisteredProcessTree(const qint64 processGroup)
                : processGroup_(processGroup) {
                const std::scoped_lock lock(processRegistryMutex);
                processRegistry.insert(processGroup_);
            }

            ~RegisteredProcessTree() {
                const std::scoped_lock lock(processRegistryMutex);
                processRegistry.erase(processGroup_);
            }

            RegisteredProcessTree(const RegisteredProcessTree&) = delete;
            RegisteredProcessTree& operator=(const RegisteredProcessTree&) = delete;

          private:
            qint64 processGroup_{0};
        };
#endif

        class ProcessStartRegistration final {
          public:
            ProcessStartRegistration() {
                const std::scoped_lock lock(processRegistryMutex);
                if (!forceStopRequested.load(std::memory_order_relaxed)) {
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
                JOBOBJECT_BASIC_ACCOUNTING_INFORMATION information{};
                return handle_ != nullptr &&
                       QueryInformationJobObject(handle_, JobObjectBasicAccountingInformation,
                                                 &information, sizeof(information), nullptr) != 0 &&
                       information.ActiveProcesses > 0;
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
                                                  PROC_THREAD_ATTRIBUTE_JOB_LIST, &jobHandle,
                                                  sizeof(jobHandle), nullptr, nullptr) != 0;
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
        process.start();
        while (process.state() == QProcess::Starting) {
            const bool forced = forceStopRequested.load(std::memory_order_relaxed);
            if (stopToken.stop_requested() || forced) {
                process.waitForStarted(static_cast<int>(kPollInterval.count()));
                const auto processGroup = process.processId();
#ifdef Q_OS_WIN
                const auto stopped = terminateTree(process, job, processGroup);
#else
                const auto stopped = terminateTree(process, processGroup);
#endif
                if (!stopped) {
                    forceStopFailed.store(true, std::memory_order_relaxed);
                }
                return {stopped ? SupervisedProcessStatus::Canceled
                                : SupervisedProcessStatus::FailedToStop,
                        process.exitCode(), diagnosticText(process, standardError, standardOutput)};
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                process.waitForStarted(static_cast<int>(kPollInterval.count()));
                const auto processGroup = process.processId();
#ifdef Q_OS_WIN
                const auto stopped = terminateTree(process, job, processGroup);
#else
                const auto stopped = terminateTree(process, processGroup);
#endif
                if (!stopped) {
                    forceStopFailed.store(true, std::memory_order_relaxed);
                }
                return {stopped ? SupervisedProcessStatus::TimedOut
                                : SupervisedProcessStatus::FailedToStop,
                        process.exitCode(), diagnosticText(process, standardError, standardOutput)};
            }
            process.waitForStarted(static_cast<int>(kPollInterval.count()));
            drainProcess(process, standardError, standardOutput);
        }
        if (process.state() == QProcess::NotRunning) {
            drainProcess(process, standardError, standardOutput);
            return {SupervisedProcessStatus::StartFailed, process.exitCode(),
                    diagnosticText(process, standardError, standardOutput)};
        }
        const auto processGroup = process.processId();
#ifdef Q_OS_WIN
        const RegisteredProcessTree registeredTree{job.get()};
#else
        const RegisteredProcessTree registeredTree{processGroup};
#endif
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
                    forceStopFailed.store(true, std::memory_order_relaxed);
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
                    forceStopFailed.store(true, std::memory_order_relaxed);
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
                forceStopFailed.store(true, std::memory_order_relaxed);
            }
            return {stopped ? SupervisedProcessStatus::Canceled
                            : SupervisedProcessStatus::FailedToStop,
                    process.exitCode(), diagnosticText(process, standardError, standardOutput)};
        }

#ifdef Q_OS_WIN
        const auto residualTree = treeActive(job, processGroup);
        const auto stoppedResidual = !residualTree || terminateTree(process, job, processGroup);
#else
        const auto residualTree = treeActive(processGroup);
        const auto stoppedResidual = !residualTree || terminateTree(process, processGroup);
#endif
        if (!stoppedResidual) {
            forceStopFailed.store(true, std::memory_order_relaxed);
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
        for (const auto job : processRegistry) {
            stopped = TerminateJobObject(job, 1) != 0 && stopped;
        }
#else
        for (const auto processGroup : processRegistry) {
            if (::kill(-static_cast<pid_t>(processGroup), SIGKILL) != 0 && errno != ESRCH) {
                stopped = false;
            }
        }
#endif
        if (!stopped) {
            forceStopFailed.store(true, std::memory_order_relaxed);
        }
        if (forceStopFailed.load(std::memory_order_relaxed)) {
            return ForceStopRequestStatus::Failed;
        }
        return processStartsInFlight == 0 ? ForceStopRequestStatus::Ready
                                          : ForceStopRequestStatus::PendingStart;
    }

    bool SupervisedProcess::forceStopAll() {
        if (requestForceStopAll() == ForceStopRequestStatus::Failed) {
            return false;
        }
        const auto deadline = std::chrono::steady_clock::now() + kForcedStopTimeout;
        while (std::chrono::steady_clock::now() < deadline) {
            const bool active = [&] {
                const std::scoped_lock lock(processRegistryMutex);
#ifdef Q_OS_WIN
                return processStartsInFlight > 0 ||
                       std::ranges::any_of(processRegistry, [](const HANDLE job) {
                           JOBOBJECT_BASIC_ACCOUNTING_INFORMATION information{};
                           return QueryInformationJobObject(
                                      job, JobObjectBasicAccountingInformation, &information,
                                      sizeof(information), nullptr) != 0 &&
                                  information.ActiveProcesses > 0;
                       });
#else
                return processStartsInFlight > 0 ||
                       std::ranges::any_of(processRegistry, treeActive);
#endif
            }();
            if (!active) {
                forceStopRequested.store(false, std::memory_order_relaxed);
                forceStopFailed.store(false, std::memory_order_relaxed);
                return true;
            }
            QThread::msleep(static_cast<unsigned long>(kPollInterval.count()));
        }
        return false;
    }

} // namespace ssa::platform
