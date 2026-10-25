#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace numerick {

// Stores completed turns in memory. Tool-call and tool-result messages stay
// grouped with the user turn that produced them.
class ConversationSession {
public:
    explicit ConversationSession(std::size_t maxTurns = 10);

    // Builds system + bounded history + the current user message.
    nlohmann::json buildMessages(
        const std::string& prompt,
        const std::string& systemPrompt
    ) const;

    // Commits a completed assistant transcript for the given user prompt.
    // assistantMessages may include assistant tool-call messages and tool
    // results, but must end with an assistant message.
    void recordTurn(
        const std::string& prompt,
        const nlohmann::json& assistantMessages
    );

    void clear() noexcept;

    std::size_t turnCount() const noexcept;
    std::size_t maxTurns() const noexcept;

private:
    std::size_t maxTurns_;
    std::vector<nlohmann::json> turns_;
};

} // namespace numerick
