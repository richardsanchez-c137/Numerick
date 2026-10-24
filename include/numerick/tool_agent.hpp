
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace numerick {

class ChatClient;
class ToolRegistry;

enum class AgentStopReason {
    Completed,
    ToolRoundLimitReached,
    ToolCallLimitReached
};

struct ToolCallDiagnostic {
    std::string name;
    std::string outcome;
};

struct AgentRunReport {
    std::string answer;
    std::size_t modelRequests = 0;
    std::size_t toolRounds = 0;
    std::size_t toolCallsRequested = 0;
    std::size_t toolCallsSucceeded = 0;
    std::size_t toolCallsFailed = 0;
    std::size_t toolCallsRejected = 0;
    AgentStopReason stopReason = AgentStopReason::Completed;
    std::vector<ToolCallDiagnostic> toolCalls;
};

class ToolAgent {
public:
    ToolAgent(
        const ChatClient& aiClient,
        const ToolRegistry& registry,
        std::size_t maxToolRounds = 3,
        std::size_t maxToolCalls = 4
    );

    // Compatibility method: returns just the final answer.
    std::string run(const std::string& prompt) const;

    // Detailed method: returns the answer and safe execution metadata.
    AgentRunReport runWithDiagnostics(
        const std::string& prompt
    ) const;

private:
    const ChatClient& aiClient_;
    const ToolRegistry& registry_;

    std::size_t maxToolRounds_;
    std::size_t maxToolCalls_;
};

} // namespace numerick
