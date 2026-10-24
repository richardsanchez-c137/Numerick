
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
    if (prompt.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument(
            "The agent prompt cannot be empty."
        );
    }

    const Json tools = makeToolDefinitions(registry_);

    Json messages = Json::array({
        {
            {"role", "system"},
            {"content",
                
                "You are NUMERICK, a personal AI assistant. "
                "Answer clearly and concisely. Use registered tools "
                "when they are the appropriate way to obtain real information. "
                "When the user asks for the current local date or time, "
                "call get_local_time and use its returned result. "
                "Never guess the current time or claim a tool succeeded "
                "unless its result confirms success. If a tool fails, "
                "explain that failure honestly. Use only the tools provided."

            }
        },
        {
            {"role", "user"},
            {"content", prompt}
        }
    });

    std::size_t toolRounds = 0;
    std::size_t toolCalls = 0;

    while (true) {
        const Json response = aiClient_.chat(messages, tools);

        if (!response.contains("message") ||
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

        // No tool request: this is the final assistant response.
        if (requestedCalls.empty()) {
            const std::string content = getMessageContent(message);

            if (content.empty()) {
                return "NUMERICK received an empty final response.";
            }

            return content;
        }

        // Stop before executing any calls that exceed our limits.
        if (toolRounds >= maxToolRounds_) {
            return "NUMERICK stopped because the tool-round limit "
                   "was reached. No additional tools were executed.";
        }

        if (toolCalls >= maxToolCalls_ ||
            requestedCalls.size() > maxToolCalls_ - toolCalls) {
            return "NUMERICK stopped because the tool-call limit "
                   "was reached. The latest requested tools were "
                   "not executed.";
        }

        // Preserve the assistant's original tool-call message.
        messages.push_back(message);

        for (const Json& call : requestedCalls) {
            std::string toolName = "__invalid_tool_call__";
            ToolResult result{
                false,
                "The AI returned a malformed tool request."
            };

            if (call.is_object() &&
                call.contains("function") &&
                call["function"].is_object()) {

                const Json& function = call["function"];

                if (function.contains("name") &&
                    function["name"].is_string()) {

                    toolName = function["name"].get<std::string>();

                    if (function.contains("arguments") &&
                        function["arguments"].is_object()) {

                        result = registry_.execute(
                            toolName,
                            function["arguments"]
                        );
                    }
                    else {
                        result = {
                            false,
                            "Tool arguments must be a JSON object."
                        };
                    }
                }
            }

            const std::string toolOutput =
                result.success
                    ? result.output
                    : "Tool error: " + result.output;

            // Send the result back to the model. A tool error is
            // data for the model, not proof that the task succeeded.
            messages.push_back({
                {"role", "tool"},
                {"tool_name", toolName},
                {"content", toolOutput}
            });

            ++toolCalls;
        }

        ++toolRounds;
    }
}

} // namespace numerick
