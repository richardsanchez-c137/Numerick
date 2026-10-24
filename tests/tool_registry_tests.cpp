
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
            {"required", {"text"}},
            {"additionalProperties", false}
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

    
    // Required arguments must be present.
    const auto missingRequired = registry.execute(
        "echo_text",
        Json::object()
    );

    assert(!missingRequired.success);
    assert(missingRequired.output.find(
        "Missing required argument: text"
    ) != std::string::npos);

    // Argument types must match the declared schema.
    const auto wrongType = registry.execute(
        "echo_text",
        {{"text", 123}}
    );

    assert(!wrongType.success);
    assert(wrongType.output.find(
        "must have type 'string'"
    ) != std::string::npos);

    // Additional arguments must be rejected when prohibited.
    const auto unexpectedArgument = registry.execute(
        "echo_text",
        {
            {"text", "hello"},
            {"admin", true}
        }
    );

    assert(!unexpectedArgument.success);
    assert(unexpectedArgument.output.find(
        "Unexpected argument: admin"
    ) != std::string::npos);


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

    
    // Invalid schemas must be rejected during registration.
    const auto rejectsInvalidSchema = [](
        const std::string& name,
        const Json& schema
    ) {
        ToolRegistry isolatedRegistry;
        bool rejected = false;

        try {
            isolatedRegistry.registerTool(
                {
                    name,
                    "An invalid schema used for testing.",
                    schema
                },
                [](const Json&) -> ToolResult {
                    return {true, "This handler must not run."};
                }
            );
        }
        catch (const std::invalid_argument&) {
            rejected = true;
        }

        assert(rejected);
        assert(!isolatedRegistry.contains(name));
    };

    // Missing top-level object type.
    const Json missingTypeSchema = {
        {"properties", Json::object()}
    };

    rejectsInvalidSchema(
        "missing_type",
        missingTypeSchema
    );

    // Properties must be declared as an object.
    const Json invalidPropertiesSchema = {
        {"type", "object"},
        {"properties", Json::array()}
    };

    rejectsInvalidSchema(
        "invalid_properties",
        invalidPropertiesSchema
    );

    // Required must be an array.
    const Json invalidRequiredSchema = {
        {"type", "object"},
        {"required", "text"}
    };

    rejectsInvalidSchema(
        "invalid_required",
        invalidRequiredSchema
    );

    // Required properties must exist in the properties definition.
    const Json undeclaredRequiredSchema = {
        {"type", "object"},
        {"properties", Json::object()},
        {"required", Json::array({"missing_field"})}
    };

    rejectsInvalidSchema(
        "undeclared_required",
        undeclaredRequiredSchema
    );

    // Unsupported property types must be rejected.
    const Json unsupportedPropertyTypeSchema = {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "file"}
            }}
        }}
    };

    rejectsInvalidSchema(
        "unsupported_property_type",
        unsupportedPropertyTypeSchema
    );

    // A property schema must itself be an object.
    const Json invalidPropertySchema = {
        {"type", "object"},
        {"properties", {
            {"text", "string"}
        }}
    };

    rejectsInvalidSchema(
        "invalid_property_schema",
        invalidPropertySchema
    );

    // additionalProperties must be a boolean.
    const Json invalidAdditionalPropertiesSchema = {
        {"type", "object"},
        {"additionalProperties", "false"}
    };

    rejectsInvalidSchema(
        "invalid_additional_properties",
        invalidAdditionalPropertiesSchema
    );


    return 0;
}
