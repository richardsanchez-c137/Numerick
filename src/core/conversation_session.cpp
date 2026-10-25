#include "numerick/conversation_session.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace numerick {
namespace {

using Json = nlohmann::json;

bool isBlank(const std::string& value) {
    return value.find_first_not_of(" \t\r\n") == std::string::npos;
}

} // namespace

ConversationSession::ConversationSession(std::size_t maxTurns)
    : maxTurns_(maxTurns) {
    if (maxTurns_ == 0) {
        throw std::invalid_argument(
            "ConversationSession maxTurns must be greater than zero."
        );
    }
}

Json ConversationSession::buildMessages(
    const std::string& prompt,
    const std::string& systemPrompt
) const {
    if (isBlank(prompt)) {
        throw std::invalid_argument(
            "The conversation prompt cannot be empty."
        );
    }

    if (isBlank(systemPrompt)) {
        throw std::invalid_argument(
            "The conversation system prompt cannot be empty."
        );
    }

    Json messages = Json::array({
        {
            {"role", "system"},
            {"content", systemPrompt}
        }
    });

    // Each stored turn is appended as a unit, so trimming never splits a
    // tool-call / tool-result sequence across separate turns.
    for (const Json& turn : turns_) {
        for (const Json& message : turn) {
            messages.push_back(message);
        }
    }

    messages.push_back({
        {"role", "user"},
        {"content", prompt}
    });

    return messages;
}

void ConversationSession::recordTurn(
    const std::string& prompt,
    const Json& assistantMessages
) {
    if (isBlank(prompt)) {
        throw std::invalid_argument(
            "The conversation prompt cannot be empty."
        );
    }

    if (!assistantMessages.is_array() || assistantMessages.empty()) {
        throw std::invalid_argument(
            "A completed turn must contain assistant messages."
        );
    }

    for (const Json& message : assistantMessages) {
        if (!message.is_object() ||
            !message.contains("role") ||
            !message["role"].is_string()) {
            throw std::invalid_argument(
                "Conversation transcript contains an invalid message."
            );
        }

        const std::string role = message["role"].get<std::string>();

        if (role != "assistant" && role != "tool") {
            throw std::invalid_argument(
                "Conversation transcript may contain only assistant "
                "and tool messages."
            );
        }

        if (role == "tool" &&
            (!message.contains("content") ||
             !message["content"].is_string())) {
            throw std::invalid_argument(
                "Tool transcript messages must contain string content."
            );
        }
    }

    const Json& finalMessage = assistantMessages.back();

    if (finalMessage["role"] != "assistant" ||
        !finalMessage.contains("content") ||
        !finalMessage["content"].is_string()) {
        throw std::invalid_argument(
            "A completed turn must end with an assistant text response."
        );
    }
    if (isBlank(finalMessage["content"].get<std::string>())) {
        throw std::invalid_argument(
            "A completed turn must end with a non-empty assistant text response."
        );
    }

    Json turn = Json::array({
        {
            {"role", "user"},
            {"content", prompt}
        }
    });

    for (const Json& message : assistantMessages) {
        turn.push_back(message);
    }

    turns_.push_back(std::move(turn));

    while (turns_.size() > maxTurns_) {
        turns_.erase(turns_.begin());
    }
}

void ConversationSession::clear() noexcept {
    turns_.clear();
}

std::size_t ConversationSession::turnCount() const noexcept {
    return turns_.size();
}

std::size_t ConversationSession::maxTurns() const noexcept {
    return maxTurns_;
}

} // namespace numerick
