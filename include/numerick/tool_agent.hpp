
#pragma once

#include <cstddef>
#include <string>

namespace numerick {

class ChatClient;
class ToolRegistry;

class ToolAgent {
public:
    ToolAgent(
        const ChatClient& aiClient,
        const ToolRegistry& registry,
        std::size_t maxToolRounds = 3,
        std::size_t maxToolCalls = 4
    );

    std::string run(const std::string& prompt) const;

private:
    const ChatClient& aiClient_;
    const ToolRegistry& registry_;

    std::size_t maxToolRounds_;
    std::size_t maxToolCalls_;
};

} // namespace numerick
