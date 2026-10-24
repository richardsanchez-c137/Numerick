
#include "numerick/chat_client.hpp"
#include "numerick/tool_agent.hpp"
#include "numerick/tool_registry.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using numerick::ChatClient;
using numerick::Json;
using numerick::ToolAgent;
using numerick::ToolDefinition;
using numerick::ToolRegistry;
using numerick::ToolResult;

class FakeChatClient final : public ChatClient {
public:
    explicit FakeChatClient(std::vector<Json> responses)
        : responses_(std::move(responses)) {}

    Json chat(
        const Json& messages,
        const Json& tools = Json::array()
    ) const override {
        seenMessages.push_back(messages);
        seenTools.push_back(tools);

        if (nextResponse_ >= responses_.size()) {
            throw std::runtime_error(
                "Fake AI has no more scripted responses."
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

static Json toolRequest(
    const std::string& name,
    const Json& arguments
) {
    return {
        {"message", {
            {"role", "assistant"},
            {"content", ""},
            {"tool_calls", Json::array({
                {
                    {"function", {
                        {"name", name},
                        {"arguments", arguments}
                    }}
                }
            })}
        }}
    };
}

static Json finalAnswer(const std::string& content) {
    return {
        {"message", {
            {"role", "assistant"},
            {"content", content}
        }}
    };
}

static void registerEchoTool(ToolRegistry& registry) {
    registry.registerTool(
        {
            "echo_text",
            "Returns text for testing purposes.",
            {
                {"type", "object"},
                {"properties", {
                    {"text", {{"type", "string"}}}
                }},
                {"required", {"text"}}
            }
        },
        [](const Json& args) -> ToolResult {
            if (!args.contains("text") ||
                !args["text"].is_string()) {
                return {false, "Invalid text argument."};
            }

            return {
                true,
                args["text"].get<std::string>()
            };
        }
    );
}

static void testSuccessfulToolCycle() {
    ToolRegistry registry;
    registerEchoTool(registry);

    FakeChatClient ai({
        toolRequest("echo_text", {{"text", "verified result"}}),
        finalAnswer("The tool returned verified result.")
    });

    ToolAgent agent(ai, registry);

    const std::string answer = agent.run("Run the echo tool.");

    assert(answer == "The tool returned verified result.");
    assert(ai.seenMessages.size() == 2);
    assert(ai.seenTools[0].size() == 1);
    assert(ai.seenTools[0][0]["function"]["name"] == "echo_text");

    const Json& followUp = ai.seenMessages[1];
    assert(followUp.back()["role"] == "tool");
    assert(followUp.back()["tool_name"] == "echo_text");
    assert(followUp.back()["content"] == "verified result");
}

static void testUnknownToolIsRejected() {
    ToolRegistry registry;
    registerEchoTool(registry);

    FakeChatClient ai({
        toolRequest("unknown_operation", Json::object()),
        finalAnswer("The requested tool was unavailable.")
    });

    ToolAgent agent(ai, registry);

    const std::string answer =
        agent.run("Request an unknown tool.");

    assert(answer == "The requested tool was unavailable.");
    assert(ai.seenMessages.size() == 2);

    const Json& resultMessage = ai.seenMessages[1].back();

    assert(resultMessage["role"] == "tool");
    assert(resultMessage["content"].get<std::string>().find(
        "Tool error: Tool is not registered"
    ) != std::string::npos);
}

static void testMalformedArgumentsAreRejected() {
    ToolRegistry registry;
    registerEchoTool(registry);

    FakeChatClient ai({
        toolRequest("echo_text", Json::array({"not", "an object"})),
        finalAnswer("The tool arguments were invalid.")
    });

    ToolAgent agent(ai, registry);

    const std::string answer =
        agent.run("Test malformed tool arguments.");

    assert(answer == "The tool arguments were invalid.");

    const Json& resultMessage = ai.seenMessages[1].back();

    assert(resultMessage["content"] ==
           "Tool error: Tool arguments must be a JSON object.");
}

static void testRoundLimitPreventsExecution() {
    ToolRegistry registry;
    int executionCount = 0;

    registry.registerTool(
        {
            "count_execution",
            "Counts executions in a test.",
            {{"type", "object"}}
        },
        [&executionCount](const Json&) -> ToolResult {
            ++executionCount;
            return {true, "executed"};
        }
    );

    FakeChatClient ai({
        toolRequest("count_execution", Json::object())
    });

    ToolAgent agent(ai, registry, 0, 4);

    const std::string answer =
        agent.run("Attempt a tool request.");

    assert(answer.find("tool-round limit") != std::string::npos);
    assert(executionCount == 0);
    assert(ai.seenMessages.size() == 1);
}

static void testCallLimitPreventsExecution() {
    ToolRegistry registry;
    int executionCount = 0;

    registry.registerTool(
        {
            "count_execution",
            "Counts executions in a test.",
            {{"type", "object"}}
        },
        [&executionCount](const Json&) -> ToolResult {
            ++executionCount;
            return {true, "executed"};
        }
    );

    Json response = {
        {"message", {
            {"role", "assistant"},
            {"content", ""},
            {"tool_calls", Json::array({
                {
                    {"function", {
                        {"name", "count_execution"},
                        {"arguments", Json::object()}
                    }}
                },
                {
                    {"function", {
                        {"name", "count_execution"},
                        {"arguments", Json::object()}
                    }}
                }
            })}
        }}
    };

    FakeChatClient ai({response});
    ToolAgent agent(ai, registry, 3, 1);

    const std::string answer =
        agent.run("Request too many calls.");

    assert(answer.find("tool-call limit") != std::string::npos);
    assert(executionCount == 0);
}

static void testEmptyPromptIsRejected() {
    ToolRegistry registry;
    FakeChatClient ai({});
    ToolAgent agent(ai, registry);

    bool rejected = false;

    try {
        agent.run("   \t ");
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
    assert(ai.seenMessages.empty());
}

int main() {
    testSuccessfulToolCycle();
    testUnknownToolIsRejected();
    testMalformedArgumentsAreRejected();
    testRoundLimitPreventsExecution();
    testCallLimitPreventsExecution();
    testEmptyPromptIsRejected();

    std::cout << "All tool agent tests passed.\n";
    return 0;
}
