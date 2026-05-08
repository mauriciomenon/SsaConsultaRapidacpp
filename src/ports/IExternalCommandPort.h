#pragma once

#include <map>
#include <string>
#include <vector>

namespace ssa::ports {

    class IExternalCommandPort {
      public:
        virtual ~IExternalCommandPort() = default;

        virtual void openSamHome() = 0;
        virtual void openSsa(const std::string& numeroSsa) = 0;
        virtual void openPath(const std::string& path) = 0;
        virtual void
        exportSelection(const std::vector<std::map<std::string, std::string>>& rows) = 0;
        virtual void requestCommand(const std::string& command,
                                    const std::map<std::string, std::string>& parameters) = 0;
    };

} // namespace ssa::ports
