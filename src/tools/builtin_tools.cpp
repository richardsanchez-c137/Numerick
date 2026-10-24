#include "numerick/builtin_tools.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace numerick {
namespace {

std::string getLocalTimeString() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t current =
        std::chrono::system_clock::to_time_t(now);

    std::tm local{};

#ifdef _WIN32
    if (localtime_s(&local, &current) != 0) {
        return "";
    }
#else
    if (localtime_r(&current, &local) == nullptr) {
        return "";
    }
#endif

    std::ostringstream output;
    output << std::put_time(
        &local, "%Y-%m-%d %H:%M:%S"
    );

    return output.str();
}

} // namespace

void registerBuiltinTools(ToolRegistry& registry) {
    registry.registerTool(
        {
            "get_local_time",
            "Returns the computer's current local date and time.",
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()},
                {"additionalProperties", false}
            }
        },
        [](const Json& arguments) -> ToolResult {
            if (!arguments.empty()) {
                return {
                    false,
                    "get_local_time does not accept arguments."
                };
            }

            const std::string timestamp = getLocalTimeString();

            if (timestamp.empty()) {
                return {
                    false,
                    "Unable to determine the local system time."
                };
            }

            return {
                true,
                timestamp
            };
        }
    );
}

} // namespace numerick
