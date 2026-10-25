#include "numerick/chat_client.hpp"
#include "numerick/tool_agent.hpp"
#include "numerick/tool_registry.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Json = nlohmann::json;
using numerick::ChatClient;
using numerick::ToolAgent;
using numerick::ToolRegistry;
using numerick::ToolResult;

class ScriptedChatClient final : public ChatClient {
public:
    explicit ScriptedChatClient(std::vector<Json> responses)
        : responses_(std::move(responses)) {}

    Json chat(
        const Json& messages,
        const Json& tools = Json::array()
    ) const override {
        seenMessages.push_back(messages);
        seenTools.push_back(tools);

        if (nextResponse_ >= responses_.size()) {
            throw std::runtime_error(
                "No scripted model response remains."
            );
        }

        return responses_[nextResponse_++];
    }

    mutable std::vector<Json> seenMessages;
    mutable std::vector<Json> seenTools;

private:
    std::vector<Json> responses_;
    mutable std::size_t nextResponse_ = 0;
};

static Json initialConversation() {
    return Json::array({
        {
            {"role", "system"},
            {"content", "You are NUMERICK for this test."}
        },
        {
            {"role", "user"},
            {"content", "Current test request"}
        }
    });
}

static Json finalAnswer(const std::string& content) {
    return {
        {"message", {
            {"role", "assistant"},
            {"content", content}
        }}
    };
}

static Json toolRequest(const Json& calls) {
    return {
        {"message", {
            {"role", "assistant"},
            {"content", ""},
            {"tool_calls", calls}
        }}
    };
}

static Json echoToolCall() {
    return {
        {"id", "call_echo"},
        {"type", "function"},
        {"function", {
            {"name", "echo_text"},
            {"arguments", {{"text", "tool output"}}}
        }}
    };
}

static void registerEchoTool(ToolRegistry& registry) {
    registry.registerTool(
        {
            "echo_text",
            "Returns supplied text for this test.",
            {
                {"type", "object"},
                {"properties", {
                    {"text", {{"type", "string"}}}
                }},
                {"required", {"text"}},
                {"additionalProperties", false}
            }
        },
        [](const Json& arguments) -> ToolResult {
            return {true, arguments["text"].get<std::string>()};
        }
    );
}

static void testDirectAnswerTranscript() {
    ToolRegistry registry;
    ScriptedChatClient ai({
        finalAnswer("Direct answer.")
    });
    ToolAgent agent(ai, registry);

    const Json initial = initialConversation();
    const auto report = agent.runWithConversationDiagnostics(initial);

    assert(report.answer == "Direct answer.");
    assert(report.turnMessages.size() == 1);
    assert(report.turnMessages[0]["role"] == "assistant");
    assert(report.turnMessages[0]["content"] == "Direct answer.");
    assert(ai.seenMessages.size() == 1);
    assert(ai.seenMessages[0] == initial);
}

static void testToolCallAndResultAppearInTranscript() {
    ToolRegistry registry;
    registerEchoTool(registry);

    ScriptedChatClient ai({
        toolRequest(Json::array({echoToolCall()})),
        finalAnswer("The tool returned tool output.")
    });

    ToolAgent agent(ai, registry);
    const auto report =
        agent.runWithConversationDiagnostics(initialConversation());

    assert(report.answer == "The tool returned tool output.");
    assert(report.modelRequests == 2);
    assert(report.toolCallsSucceeded == 1);

    assert(report.turnMessages.size() == 3);
    assert(report.turnMessages[0]["role"] == "assistant");
    assert(report.turnMessages[0].contains("tool_calls"));
    assert(report.turnMessages[1]["role"] == "tool");
    assert(report.turnMessages[1]["tool_name"] == "echo_text");
    assert(report.turnMessages[1]["content"] == "tool output");
    assert(report.turnMessages[2]["role"] == "assistant");
    assert(
        report.turnMessages[2]["content"] ==
        "The tool returned tool output."
    );

    // The second model request received the tool-call message and tool result.
    assert(ai.seenMessages[1].size() == initialConversation().size() + 2);
    assert(ai.seenMessages[1][2]["role"] == "assistant");
    assert(ai.seenMessages[1][3]["role"] == "tool");
}

static void testLimitStopStillReturnsARecordableAssistantMessage() {
    ToolRegistry registry;
    registerEchoTool(registry);

    ScriptedChatClient ai({
        toolRequest(Json::array({echoToolCall()}))
    });

    ToolAgent agent(ai, registry, 0, 4);
    const auto report =
        agent.runWithConversationDiagnostics(initialConversation());

    assert(
        report.stopReason ==
        numerick::AgentStopReason::ToolRoundLimitReached
    );
    assert(report.turnMessages.size() == 1);
    assert(report.turnMessages[0]["role"] == "assistant");
    assert(report.turnMessages[0]["content"] == report.answer);
}

static void testRejectsInvalidConversationShape() {
    ToolRegistry registry;
    ScriptedChatClient ai({
        finalAnswer("This should not be requested.")
    });
    ToolAgent agent(ai, registry);

    bool rejected = false;

    try {
        agent.runWithConversationDiagnostics(Json::array({
            {{"role", "user"}, {"content", "Missing system message"}}
        }));
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
    assert(ai.seenMessages.empty());
}

static void testEmptyAssistantResponseThrowsInsteadOfReturningSuccess() {
    ToolRegistry registry;
    ScriptedChatClient ai({finalAnswer(" \t\r\n")});
    ToolAgent agent(ai, registry);

    bool failed = false;
    try {
        (void)agent.runWithConversationDiagnostics(initialConversation());
    }
    catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);
}

int main() {
    std::cout << "Running direct-answer test..." << std::endl;
    testDirectAnswerTranscript();

    std::cout << "Running tool-transcript test..." << std::endl;
    testToolCallAndResultAppearInTranscript();

    std::cout << "Running execution-limit test..." << std::endl;
    testLimitStopStillReturnsARecordableAssistantMessage();

    std::cout << "Running invalid-conversation test..." << std::endl;
    testRejectsInvalidConversationShape();
    testEmptyAssistantResponseThrowsInsteadOfReturningSuccess();

    std::cout << "All tool-agent conversation tests passed.\n";
    return 0;
}
