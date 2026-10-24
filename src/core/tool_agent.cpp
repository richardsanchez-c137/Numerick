
#include "numerick/tool_agent.hpp"

#include "numerick/chat_client.hpp"
#include "numerick/tool_registry.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace numerick {
namespace {

using Json = nlohmann::json;

Json makeToolDefinitions(const ToolRegistry& registry) {
    Json tools = Json::array();

    for (const ToolDefinition& definition : registry.definitions()) {
        tools.push_back({
            {"type", "function"},
            {"function", {
                {"name", definition.name},
                {"description", definition.description},
                {"parameters", definition.parametersSchema}
            }}
        });
    }

    return tools;
}

std::string getMessageContent(const Json& message) {
    if (message.contains("content") &&
        message["content"].is_string()) {
        return message["content"].get<std::string>();
    }

    return "";
}

} // namespace

ToolAgent::ToolAgent(
    const ChatClient& aiClient,
    const ToolRegistry& registry,
    std::size_t maxToolRounds,
    std::size_t maxToolCalls
)
    : aiClient_(aiClient),
      registry_(registry),
      maxToolRounds_(maxToolRounds),
      maxToolCalls_(maxToolCalls) {}

std::string ToolAgent::run(const std::string& prompt) const {
    return runWithDiagnostics(prompt).answer;
}

AgentRunReport ToolAgent::runWithDiagnostics(
    const std::string& prompt
) const {
    if (prompt.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument(
            "The agent prompt cannot be empty."
        );
    }

    AgentRunReport report;
    const Json tools = makeToolDefinitions(registry_);

    Json messages = Json::array({
        {
            {"role", "system"},
            {"content",
                "You are NUMERICK, a personal AI assistant. "
                "Answer clearly and concisely. Use registered tools "
                "when they are appropriate for obtaining real "
                "information. When asked for the current local time, "
                "call get_local_time and use its returned result. "
                "Never guess the current time or claim a tool "
                "succeeded without its result. If a tool fails, "
                "explain that failure honestly. Use only the tools "
                "provided with this request."
            }
        },
        {
            {"role", "user"},
            {"content", prompt}
        }
    });

    std::size_t processedCalls = 0;

    while (true) {
        const Json response = aiClient_.chat(messages, tools);
        ++report.modelRequests;

        if (!response.is_object() ||
            !response.contains("message") ||
            !response["message"].is_object()) {
            throw std::runtime_error(
                "The AI response does not contain a valid message."
            );
        }

        const Json& message = response["message"];
        Json requestedCalls = Json::array();

        if (message.contains("tool_calls")) {
            if (!message["tool_calls"].is_array()) {
                throw std::runtime_error(
                    "The AI returned an invalid tool-call list."
                );
            }

            requestedCalls = message["tool_calls"];
        }

        if (requestedCalls.empty()) {
            report.answer = getMessageContent(message);

            if (report.answer.empty()) {
                report.answer =
                    "NUMERICK received an empty final response.";
            }

            return report;
        }

        report.toolCallsRequested += requestedCalls.size();

        if (report.toolRounds >= maxToolRounds_) {
            report.stopReason = AgentStopReason::ToolRoundLimitReached;
            report.answer =
                "NUMERICK stopped because the tool-round limit "
                "was reached. No additional tools were executed.";
            return report;
        }

        if (processedCalls > maxToolCalls_ ||
            requestedCalls.size() > maxToolCalls_ - processedCalls) {
            report.stopReason = AgentStopReason::ToolCallLimitReached;
            report.answer =
                "NUMERICK stopped because the tool-call limit "
                "was reached. The latest requested tools were "
                "not executed.";
            return report;
        }

        // Preserve the assistant message that contains tool_calls.
        messages.push_back(message);
        ++report.toolRounds;

        for (const Json& call : requestedCalls) {
            std::string toolName = "__invalid_tool_call__";
            std::string outcome = "rejected";
            ToolResult result{
                false,
                "Malformed tool request."
            };

            bool wellFormed = false;

            if (call.is_object() &&
                call.contains("function") &&
                call["function"].is_object()) {

                const Json& function = call["function"];

                if (function.contains("name") &&
                    function["name"].is_string() &&
                    function.contains("arguments") &&
                    function["arguments"].is_object()) {

                    toolName = function["name"].get<std::string>();
                    wellFormed = true;
                }
            }

            if (!wellFormed) {
                outcome = "malformed";
                ++report.toolCallsRejected;
                result.output =
                    "The tool request was malformed. "
                    "A string name and object arguments are required.";
            }
            else if (!registry_.contains(toolName)) {
                outcome = "rejected";
                ++report.toolCallsRejected;
                result.output =
                    "Tool is not registered: " + toolName;
            }
            else {
                result = registry_.execute(
                    toolName,
                    call["function"]["arguments"]
                );

                // Count a registry submission as processed even when
                // schema validation or the handler rejects it.
                if (result.success) {
                    outcome = "succeeded";
                    ++report.toolCallsSucceeded;
                }
                else {
                    outcome = "failed";
                    ++report.toolCallsFailed;
                }
            }

            messages.push_back({
                {"role", "tool"},
                {"tool_name", toolName},
                {"content",
                    result.success
                        ? result.output
                        : "Tool error: " + result.output
                }
            });

            report.toolCalls.push_back({toolName, outcome});
            ++processedCalls;
        }
    }
}

} // namespace numerick
