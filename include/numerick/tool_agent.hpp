
#pragma once

#include <cstddef>
#include <string>

namespace numerick {

class AIClient;
class ToolRegistry;

class ToolAgent {
public:
    ToolAgent(
        AIClient& aiClient,
        const ToolRegistry& registry,
        std::size_t maxToolRounds = 3,
        std::size_t maxToolCalls = 4
    );

    // Run one user request through the AI/tool execution loop.
    std::string run(const std::string& prompt) const;

private:
    AIClient& aiClient_;
    const ToolRegistry& registry_;

    std::size_t maxToolRounds_;
    std::size_t maxToolCalls_;
};

} // namespace numerick
