#include "numerick/tool_agent.hpp"

#include "numerick/chat_client.hpp"
#include "numerick/tool_registry.hpp"

#include <cctype>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace numerick {
namespace {

using Json = nlohmann::json;

constexpr const char* kAgentSystemPrompt =
    "You are NUMERICK, a personal AI assistant. "
    "Answer clearly and concisely. Use registered tools "
    "when they are appropriate for obtaining real "
    "information. When asked for the current local time, "
    "call get_local_time and use its returned result. "
    "Never guess the current time or claim a tool "
    "succeeded without its result. If a tool fails, "
    "explain that failure honestly. Use only the tools "
    "provided with this request.";

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

bool isPromptIdentifierCharacter(unsigned char character) {
    return std::isalnum(character) != 0 || character == '_';
}

bool promptContainsIdentifier(const std::string& prompt, const std::string& identifier) {
    if (identifier.empty()) {
        return false;
    }

    std::size_t position = prompt.find(identifier);
    while (position != std::string::npos) {
        const bool leftBoundary = position == 0 ||
            !isPromptIdentifierCharacter(static_cast<unsigned char>(prompt[position - 1]));
        const std::size_t after = position + identifier.size();
        const bool rightBoundary = after >= prompt.size() ||
            !isPromptIdentifierCharacter(static_cast<unsigned char>(prompt[after]));
        if (leftBoundary && rightBoundary) {
            return true;
        }
        position = prompt.find(identifier, position + 1);
    }
    return false;
}

std::string relationshipArgumentMismatch(
    const Json& arguments,
    const std::string& currentUserPrompt
) {
    const char* keys[] = {"class_a", "member_a", "class_b", "member_b"};
    for (const char* key : keys) {
        if (!arguments.contains(key) || !arguments[key].is_string()) {
            // Let ToolRegistry's JSON-schema validation report missing/wrongly typed arguments.
            return {};
        }
        const std::string value = arguments[key].get<std::string>();
        if (!promptContainsIdentifier(currentUserPrompt, value)) {
            return "Relationship lookup argument '" + std::string(key) + "'='" + value +
                "' was not mentioned in the current user request. Use the exact class/member "
                "identifiers from the user's question; do not substitute a test fixture or a "
                "similarly named type.";
        }
    }
    return {};
}

std::vector<std::pair<std::string, std::string>> extractClassMemberPairs(
    const std::string& prompt
) {
    // Accept natural possessives (Numerick's registry_) and C++-style pairs
    // (Numerick::registry_). Only bare identifier tokens are captured.
    const std::regex pairPattern(
        R"(([A-Za-z_][A-Za-z0-9_]*)\s*(?:'s|::)\s*([A-Za-z_][A-Za-z0-9_]*))"
    );
    std::vector<std::pair<std::string, std::string>> pairs;
    for (std::sregex_iterator match(prompt.begin(), prompt.end(), pairPattern), end;
         match != end; ++match) {
        pairs.emplace_back((*match)[1].str(), (*match)[2].str());
        if (pairs.size() >= 4) {
            break;
        }
    }
    return pairs;
}

bool requestsMemberRelationshipEvidence(const std::string& prompt) {
    std::string lower = prompt;
    for (char& character : lower) {
        character = static_cast<char>(std::tolower(
            static_cast<unsigned char>(character)
        ));
    }

    const char* indicators[] = {
        "relationship", "ownership", "reference", "by-value", "by value",
        "separate object", "separate objects", "same object", "same instance",
        "alias", "member refers", "object or reference"
    };
    for (const char* indicator : indicators) {
        if (lower.find(indicator) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void captureTurnMessages(
    AgentRunReport& report,
    const Json& messages,
    std::size_t initialMessageCount
) {
    report.turnMessages = Json::array();

    for (std::size_t i = initialMessageCount; i < messages.size(); ++i) {
        report.turnMessages.push_back(messages[i]);
    }
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

    const Json messages = Json::array({
        {
            {"role", "system"},
            {"content", kAgentSystemPrompt}
        },
        {
            {"role", "user"},
            {"content", prompt}
        }
    });

    return runWithConversationDiagnostics(messages);
}

AgentRunReport ToolAgent::runWithConversationDiagnostics(
    const Json& conversationMessages
) const {
    if (!conversationMessages.is_array() ||
        conversationMessages.size() < 2) {
        throw std::invalid_argument(
            "Agent conversation must contain a system message and "
            "a current user message."
        );
    }

    if (!conversationMessages.front().is_object() ||
        !conversationMessages.front().contains("role") ||
        conversationMessages.front()["role"] != "system") {
        throw std::invalid_argument(
            "The first agent conversation message must be a system message."
        );
    }

    const Json& lastMessage = conversationMessages.back();

    if (!lastMessage.is_object() ||
        !lastMessage.contains("role") ||
        lastMessage["role"] != "user" ||
        !lastMessage.contains("content") ||
        !lastMessage["content"].is_string() ||
        lastMessage["content"].get<std::string>().find_first_not_of(
            " \t\r\n"
        ) == std::string::npos) {
        throw std::invalid_argument(
            "The agent conversation must end with a non-empty user message."
        );
    }

    AgentRunReport report;
    const Json tools = makeToolDefinitions(registry_);
    Json messages = conversationMessages;
    const std::size_t initialMessageCount = messages.size();

    std::size_t processedCalls = 0;
    std::string memberRelationshipFailure;

    // Narrow, deterministic routing for explicit C++ member-relationship questions.
    // This prevents the model from substituting a generic symbol search or unrelated test fixture.
    const std::string currentPrompt = lastMessage["content"].get<std::string>();
    const auto memberPairs = extractClassMemberPairs(currentPrompt);
    if (registry_.contains("inspect_cpp_member_relationship") &&
        memberPairs.size() >= 2 &&
        requestsMemberRelationshipEvidence(currentPrompt)) {
        const Json arguments = {
            {"class_a", memberPairs[0].first},
            {"member_a", memberPairs[0].second},
            {"class_b", memberPairs[1].first},
            {"member_b", memberPairs[1].second}
        };
        const ToolResult relationship = registry_.execute(
            "inspect_cpp_member_relationship", arguments
        );

        report.toolCallsRequested = 1;
        report.toolCalls.push_back({
            "inspect_cpp_member_relationship",
            relationship.success ? "succeeded" : "failed"
        });
        if (relationship.success) {
            report.toolCallsSucceeded = 1;
            report.answer = relationship.output;
        }
        else {
            report.toolCallsFailed = 1;
            report.answer =
                "NUMERICK could not verify the requested C++ member relationship. " +
                relationship.output;
        }

        // This targeted path doesn't ask the model to paraphrase source evidence.
        // Keep the answer as a completed assistant turn in conversation history.
        report.turnMessages = Json::array();
        report.turnMessages.push_back({
            {"role", "assistant"},
            {"content", report.answer}
        });
        return report;
    }

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
            // If the specialist relationship tool failed, do not let a generic model answer
            // substitute for evidence that was not obtained.
            if (!memberRelationshipFailure.empty()) {
                report.answer =
                    "NUMERICK could not verify the requested C++ member relationship. " +
                    memberRelationshipFailure;
            }
            else {
                report.answer = getMessageContent(message);
            }

            if (report.answer.empty()) {
                report.answer =
                    "NUMERICK received an empty final response.";
            }

            Json finalMessage = message;
            finalMessage["role"] = "assistant";
            finalMessage["content"] = report.answer;
            messages.push_back(std::move(finalMessage));

            captureTurnMessages(
                report,
                messages,
                initialMessageCount
            );
            return report;
        }

        report.toolCallsRequested += requestedCalls.size();

        if (report.toolRounds >= maxToolRounds_) {
            report.stopReason = AgentStopReason::ToolRoundLimitReached;
            report.answer =
                "NUMERICK stopped because the tool-round limit "
                "was reached. No additional tools were executed.";

            messages.push_back({
                {"role", "assistant"},
                {"content", report.answer}
            });

            captureTurnMessages(
                report,
                messages,
                initialMessageCount
            );
            return report;
        }

        if (processedCalls > maxToolCalls_ ||
            requestedCalls.size() > maxToolCalls_ - processedCalls) {
            report.stopReason = AgentStopReason::ToolCallLimitReached;
            report.answer =
                "NUMERICK stopped because the tool-call limit "
                "was reached. The latest requested tools were "
                "not executed.";

            messages.push_back({
                {"role", "assistant"},
                {"content", report.answer}
            });

            captureTurnMessages(
                report,
                messages,
                initialMessageCount
            );
            return report;
        }

        // Preserve the assistant message containing the tool-call request.
        Json assistantToolMessage = message;
        assistantToolMessage["role"] = "assistant";
        messages.push_back(std::move(assistantToolMessage));
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
                const Json& arguments = call["function"]["arguments"];
                if (toolName == "inspect_cpp_member_relationship") {
                    const std::string mismatch = relationshipArgumentMismatch(
                        arguments,
                        lastMessage["content"].get<std::string>()
                    );
                    if (!mismatch.empty()) {
                        result = {false, mismatch};
                    }
                    else {
                        result = registry_.execute(toolName, arguments);
                    }
                    if (!result.success) {
                        memberRelationshipFailure = result.output;
                    }
                }
                else {
                    result = registry_.execute(toolName, arguments);
                }

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

            // This tool produces a bounded, evidence-bearing report intended to be the
            // answer. Return it directly so the model cannot contradict the source evidence
            // while paraphrasing it (for example, saying "separate objects" for a reference).
            if (toolName == "inspect_cpp_member_relationship" && result.success) {
                report.answer = result.output;
                messages.push_back({
                    {"role", "assistant"},
                    {"content", report.answer}
                });
                captureTurnMessages(report, messages, initialMessageCount);
                return report;
            }
        }
    }
}

} // namespace numerick
