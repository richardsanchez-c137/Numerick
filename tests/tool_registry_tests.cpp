
#include "numerick/tool_registry.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

using numerick::Json;
using numerick::ToolDefinition;
using numerick::ToolRegistry;
using numerick::ToolResult;

int main() {
    ToolRegistry registry;

    const ToolDefinition echoDefinition{
        "echo_text",
        "Returns the provided text for testing.",
        {
            {"type", "object"},
            {"properties", {
                {"text", {{"type", "string"}}}
            }},
            {"required", {"text"}}
        }
    };

    registry.registerTool(
        echoDefinition,
        [](const Json& arguments) -> ToolResult {
            if (!arguments.contains("text") ||
                !arguments["text"].is_string()) {
                return {
                    false,
                    "The text argument must be a string."
                };
            }

            return {
                true,
                arguments["text"].get<std::string>()
            };
        }
    );

    // Registered tools can be found.
    assert(registry.contains("echo_text"));
    assert(!registry.contains("delete_everything"));

    // Tool definitions can be inspected.
    const auto definitions = registry.definitions();
    assert(definitions.size() == 1);
    assert(definitions[0].name == "echo_text");

    // A registered handler executes successfully.
    const auto success = registry.execute(
        "echo_text",
        {{"text", "NUMERICK tool test"}}
    );

    assert(success.success);
    assert(success.output == "NUMERICK tool test");

    // Unknown tools must not execute.
    const auto unknown = registry.execute(
        "delete_everything",
        Json::object()
    );

    assert(!unknown.success);

    // Non-object arguments must be rejected.
    const auto invalidArguments = registry.execute(
        "echo_text",
        Json::array()
    );

    assert(!invalidArguments.success);

    // Invalid arguments must be handled by the registered handler.
    const auto missingText = registry.execute(
        "echo_text",
        Json::object()
    );

    assert(!missingText.success);

    // Duplicate tool registrations must be rejected.
    bool duplicateRejected = false;

    try {
        registry.registerTool(
            echoDefinition,
            [](const Json&) -> ToolResult {
                return {true, "This should never replace the tool."};
            }
        );
    }
    catch (const std::invalid_argument&) {
        duplicateRejected = true;
    }

    assert(duplicateRejected);

    // Exceptions from handlers must become failures.
    registry.registerTool(
        {
            "test_failure",
            "A controlled test for handler failures.",
            {{"type", "object"}}
        },
        [](const Json&) -> ToolResult {
            throw std::runtime_error("Expected test exception");
        }
    );

    const auto failed = registry.execute(
        "test_failure",
        Json::object()
    );

    assert(!failed.success);
    assert(failed.output.find("Expected test exception")
           != std::string::npos);

    std::cout << "All tool registry tests passed.\n";

    return 0;
}
