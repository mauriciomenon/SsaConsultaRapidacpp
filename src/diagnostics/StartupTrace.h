#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>

namespace ssa::diagnostics {

    [[nodiscard]] inline bool startupTraceEnabled() {
        static const bool enabled = [] {
            const char* value = std::getenv("SSA_STARTUP_TRACE");
            return value != nullptr && std::string_view{value} == "1";
        }();
        return enabled;
    }

    inline void traceStartupEvent(const std::string_view event,
                                  const std::string_view details = {}) {
        if (!startupTraceEnabled()) {
            return;
        }
        const auto monoNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
        std::string line = "SSA_STARTUP_TRACE event=";
        line.append(event);
        line.append(" mono_ns=");
        line.append(std::to_string(monoNs));
        if (!details.empty()) {
            line.push_back(' ');
            line.append(details);
        }
        line.push_back('\n');

        static std::mutex outputMutex;
        const std::scoped_lock lock(outputMutex);
        static_cast<void>(std::fwrite(line.data(), 1, line.size(), stderr));
        static_cast<void>(std::fflush(stderr));
    }

} // namespace ssa::diagnostics
