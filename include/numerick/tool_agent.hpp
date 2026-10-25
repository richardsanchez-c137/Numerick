#pragma once

#include <nlohmann/json.hpp>

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

    // Assistant and tool messages added during this turn. This is kept
    // separate from the prior conversation so the session can safely commit
    // only a completed turn.
    nlohmann::json turnMessages = nlohmann::json::array();
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

    // Creates a fresh system/user message pair and runs the agent.
    AgentRunReport runWithDiagnostics(
        const std::string& prompt
    ) const;

    // Runs using an existing conversation, ending with the current user
    // message. The first message must be a system message.
    AgentRunReport runWithConversationDiagnostics(
        const nlohmann::json& conversationMessages
    ) const;

private:
    const ChatClient& aiClient_;
    const ToolRegistry& registry_;

    std::size_t maxToolRounds_;
    std::size_t maxToolCalls_;
};

} // namespace numerick
