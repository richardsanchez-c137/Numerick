
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
using numerick::ToolRegistry;
using numerick::ToolResult;

// Simulates model responses so tests do not require Ollama.
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

static Json makeToolCall(
    const std::string& name,
    const Json& arguments,
    const std::string& id = "call_test"
) {
    return {
        {"id", id},
        {"type", "function"},
        {"function", {
            {"name", name},
            {"arguments", arguments}
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

static Json finalAnswer(const std::string& content) {
    return {
        {"message", {
            {"role", "assistant"},
            {"content", content}
        }}
    };
}

static void registerEchoTool(
    ToolRegistry& registry,
    int* executionCount = nullptr
) {
    registry.registerTool(
        {
            "echo_text",
            "Returns text for testing purposes.",
            {
                {"type", "object"},
                {"properties", {
                    {"text", {{"type", "string"}}}
                }},
                {"required", {"text"}},
                {"additionalProperties", false}
            }
        },
        [executionCount](const Json& arguments) -> ToolResult {
            if (executionCount != nullptr) {
                ++(*executionCount);
            }

            return {
                true,
                arguments["text"].get<std::string>()
            };
        }
    );
}

// 1. Successful tool execution and result handoff.
static void testSuccessfulToolCycle() {
    ToolRegistry registry;
    registerEchoTool(registry);

    FakeChatClient ai(std::vector<Json>{
        toolRequest(Json::array({
            makeToolCall(
                "echo_text",
                {{"text", "verified result"}}
            )
        })),
        finalAnswer("The tool returned verified result.")
    });

    ToolAgent agent(ai, registry);

    const auto report =
        agent.runWithDiagnostics("Run the echo tool.");

    assert(report.answer == "The tool returned verified result.");
    assert(report.modelRequests == 2);
    assert(report.toolRounds == 1);
    assert(report.toolCallsRequested == 1);
    assert(report.toolCallsSucceeded == 1);
    assert(report.toolCallsFailed == 0);
    assert(report.toolCallsRejected == 0);

    assert(report.toolCalls.size() == 1);
    assert(report.toolCalls[0].name == "echo_text");
    assert(report.toolCalls[0].outcome == "succeeded");

    const Json& followUp = ai.seenMessages[1];
    assert(followUp.back()["role"] == "tool");
    assert(followUp.back()["tool_name"] == "echo_text");
    assert(followUp.back()["content"] == "verified result");
}

// 2. Unknown tools must be rejected by the registry.
static void testUnknownToolIsRejected() {
    ToolRegistry registry;
    registerEchoTool(registry);

    FakeChatClient ai(std::vector<Json>{
        toolRequest(Json::array({
            makeToolCall("unknown_operation", Json::object())
        })),
        finalAnswer("The requested tool was unavailable.")
    });

    ToolAgent agent(ai, registry);

    const auto report =
        agent.runWithDiagnostics("Request an unknown tool.");

    assert(report.answer == "The requested tool was unavailable.");
    assert(report.modelRequests == 2);
    assert(report.toolCallsRequested == 1);
    assert(report.toolCallsRejected == 1);
    assert(report.toolCallsSucceeded == 0);
    assert(report.toolCalls.size() == 1);
    assert(report.toolCalls[0].outcome == "rejected");

    const Json& resultMessage = ai.seenMessages[1].back();

    assert(resultMessage["role"] == "tool");
    assert(
        resultMessage["content"].get<std::string>().find(
            "Tool error: Tool is not registered"
        ) != std::string::npos
    );
}

// 3. Arguments that are not a JSON object must be rejected.
static void testMalformedArgumentsAreRejected() {
    ToolRegistry registry;
    int executionCount = 0;
    registerEchoTool(registry, &executionCount);

    FakeChatClient ai(std::vector<Json>{
        toolRequest(Json::array({
            makeToolCall(
                "echo_text",
                Json::array({"not", "an", "object"})
            )
        })),
        finalAnswer("The tool request was invalid.")
    });

    ToolAgent agent(ai, registry);

    const auto report =
        agent.runWithDiagnostics("Test malformed arguments.");

    assert(report.answer == "The tool request was invalid.");
    assert(report.toolCallsRejected == 1);
    assert(report.toolCallsSucceeded == 0);
    assert(report.toolCalls.size() == 1);
    assert(report.toolCalls[0].outcome == "malformed");

    assert(executionCount == 0);

    const Json& resultMessage = ai.seenMessages[1].back();

    assert(
        resultMessage["content"].get<std::string>().find(
            "tool request was malformed"
        ) != std::string::npos
    );
}

// 4. Registry schema validation must prevent the handler from running.
static void testSchemaFailureIsReported() {
    ToolRegistry registry;
    int executionCount = 0;
    registerEchoTool(registry, &executionCount);

    FakeChatClient ai(std::vector<Json>{
        toolRequest(Json::array({
            makeToolCall("echo_text", Json::object())
        })),
        finalAnswer("The required tool argument was missing.")
    });

    ToolAgent agent(ai, registry);

    const auto report =
        agent.runWithDiagnostics("Test a missing required field.");

    assert(
        report.answer ==
        "The required tool argument was missing."
    );
    assert(report.toolCallsFailed == 1);
    assert(report.toolCallsSucceeded == 0);
    assert(report.toolCallsRejected == 0);
    assert(report.toolCalls.size() == 1);
    assert(report.toolCalls[0].outcome == "failed");

    assert(executionCount == 0);

    const Json& resultMessage = ai.seenMessages[1].back();

    assert(
        resultMessage["content"].get<std::string>().find(
            "Missing required argument: text"
        ) != std::string::npos
    );
}

// 5. A failed handler must not be counted as successful.
static void testHandlerFailureIsReported() {
    ToolRegistry registry;

    registry.registerTool(
        {
            "failing_tool",
            "A tool that deliberately fails in a test.",
            {{"type", "object"}}
        },
        [](const Json&) -> ToolResult {
            return {false, "Expected test failure."};
        }
    );

    FakeChatClient ai(std::vector<Json>{
        toolRequest(Json::array({
            makeToolCall("failing_tool", Json::object())
        })),
        finalAnswer(
            "The tool failed, so the task was not completed."
        )
    });

    ToolAgent agent(ai, registry);

    const auto report =
        agent.runWithDiagnostics("Run the failing tool.");

    assert(
        report.answer ==
        "The tool failed, so the task was not completed."
    );
    assert(report.toolCallsFailed == 1);
    assert(report.toolCallsSucceeded == 0);
    assert(report.toolCallsRejected == 0);
    assert(report.toolCalls.size() == 1);
    assert(report.toolCalls[0].outcome == "failed");

    const Json& resultMessage = ai.seenMessages[1].back();

    assert(
        resultMessage["content"] ==
        "Tool error: Expected test failure."
    );
}

// 6. A malformed model response must fail safely.
static void testMalformedModelResponseIsRejected() {
    ToolRegistry registry;

    FakeChatClient ai(std::vector<Json>{
        {
            {"message", {
                {"role", "assistant"},
                {"content", ""},
                {"tool_calls", "not an array"}
            }}
        }
    });

    ToolAgent agent(ai, registry);

    bool rejected = false;

    try {
        agent.run("Test a malformed model response.");
    }
    catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
    assert(ai.seenMessages.size() == 1);
}

// 7. Reaching the round limit must prevent execution.
static void testRoundLimitPreventsExecution() {
    int executionCount = 0;
    ToolRegistry registry;

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

    FakeChatClient ai(std::vector<Json>{
        toolRequest(Json::array({
            makeToolCall("count_execution", Json::object())
        }))
    });

    ToolAgent agent(ai, registry, 0, 4);

    const auto report =
        agent.runWithDiagnostics("Attempt a tool request.");

    assert(
        report.stopReason ==
        numerick::AgentStopReason::ToolRoundLimitReached
    );
    assert(report.modelRequests == 1);
    assert(report.toolCallsRequested == 1);
    assert(executionCount == 0);
    assert(ai.seenMessages.size() == 1);
}

// 8. A request exceeding the call limit must not partially execute.
static void testCallLimitPreventsExecution() {
    int executionCount = 0;
    ToolRegistry registry;

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

    FakeChatClient ai(std::vector<Json>{
        toolRequest(Json::array({
            makeToolCall(
                "count_execution", Json::object(), "call_1"
            ),
            makeToolCall(
                "count_execution", Json::object(), "call_2"
            )
        }))
    });

    ToolAgent agent(ai, registry, 3, 1);

    const auto report =
        agent.runWithDiagnostics("Request too many calls.");

    assert(
        report.stopReason ==
        numerick::AgentStopReason::ToolCallLimitReached
    );
    assert(report.toolCallsRequested == 2);
    assert(executionCount == 0);
    assert(ai.seenMessages.size() == 1);
}

// 9. Blank prompts must be rejected before contacting the client.
static void testEmptyPromptIsRejected() {
    ToolRegistry registry;
    FakeChatClient ai(std::vector<Json>{});
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


static void testDirectAnswerDoesNotInvokeTools() {
    ToolRegistry registry;

    int executionCount = 0;
    registerEchoTool(registry, &executionCount);

    FakeChatClient ai(std::vector<Json>{
        finalAnswer("An API lets software applications communicate.")
    });

    ToolAgent agent(ai, registry);

    const auto report = agent.runWithDiagnostics(
        "Explain an API in one sentence."
    );

    assert(
        report.answer ==
        "An API lets software applications communicate."
    );

    // A direct answer should require only one model request.
    assert(report.modelRequests == 1);
    assert(report.toolRounds == 0);
    assert(report.toolCallsRequested == 0);

    assert(report.toolCallsSucceeded == 0);
    assert(report.toolCallsFailed == 0);
    assert(report.toolCallsRejected == 0);
    assert(report.toolCalls.empty());

    // The registered tool must not execute unless requested.
    assert(executionCount == 0);
}


int main() {
    testSuccessfulToolCycle();
    testUnknownToolIsRejected();
    testMalformedArgumentsAreRejected();
    testSchemaFailureIsReported();
    testHandlerFailureIsReported();
    testMalformedModelResponseIsRejected();
    testRoundLimitPreventsExecution();
    testCallLimitPreventsExecution();
    testEmptyPromptIsRejected();
    testDirectAnswerDoesNotInvokeTools();

    std::cout << "All tool agent tests passed.\n";
    return 0;
}
