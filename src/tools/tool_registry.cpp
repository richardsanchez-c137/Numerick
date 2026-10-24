
#include "numerick/tool_registry.hpp"

#include <cctype>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace numerick {
namespace {

using Json = nlohmann::json;

bool isValidToolName(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    for (unsigned char character : name) {
        if (!std::isalnum(character) && character != '_') {
            return false;
        }
    }

    return true;
}

bool isSupportedType(const std::string& type) {
    return type == "string" ||
           type == "integer" ||
           type == "number" ||
           type == "boolean" ||
           type == "object" ||
           type == "array" ||
           type == "null";
}

bool matchesType(const Json& value, const std::string& type) {
    if (type == "string") {
        return value.is_string();
    }

    if (type == "integer") {
        return value.is_number_integer() ||
               value.is_number_unsigned();
    }

    if (type == "number") {
        return value.is_number();
    }

    if (type == "boolean") {
        return value.is_boolean();
    }

    if (type == "object") {
        return value.is_object();
    }

    if (type == "array") {
        return value.is_array();
    }

    if (type == "null") {
        return value.is_null();
    }

    return false;
}

// Validate the supported subset of a tool's parameter schema.
std::string validateSchemaDefinition(const Json& schema) {
    if (!schema.is_object() ||
        schema.value("type", std::string{}) != "object") {
        return "The tool schema must have type 'object'.";
    }

    Json properties = Json::object();

    if (schema.contains("properties")) {
        if (!schema["properties"].is_object()) {
            return "Schema properties must be a JSON object.";
        }

        properties = schema["properties"];
    }

    if (schema.contains("required")) {
        if (!schema["required"].is_array()) {
            return "Schema required must be a JSON array.";
        }

        for (const Json& item : schema["required"]) {
            if (!item.is_string()) {
                return "Every required entry must be a string.";
            }

            const std::string name = item.get<std::string>();

            if (!properties.contains(name)) {
                return "Required property is not declared: " + name;
            }
        }
    }

    if (schema.contains("additionalProperties") &&
        !schema["additionalProperties"].is_boolean()) {
        return "Schema additionalProperties must be a boolean.";
    }

    for (auto property = properties.begin();
         property != properties.end();
         ++property) {

        const Json& propertySchema = property.value();

        if (!propertySchema.is_object()) {
            return "Schema for property '" + property.key() +
                   "' must be an object.";
        }

        if (propertySchema.contains("type")) {
            if (!propertySchema["type"].is_string()) {
                return "Type for property '" + property.key() +
                       "' must be a string.";
            }

            const std::string type =
                propertySchema["type"].get<std::string>();

            if (!isSupportedType(type)) {
                return "Unsupported schema type '" + type +
                       "' for property '" + property.key() + "'.";
            }
        }
    }

    return "";
}

// Validate actual arguments against the supported schema subset.
std::string validateArguments(
    const Json& schema,
    const Json& arguments
) {
    if (!arguments.is_object()) {
        return "Tool arguments must be a JSON object.";
    }

    const Json properties =
        schema.value("properties", Json::object());

    if (schema.contains("required")) {
        for (const Json& item : schema["required"]) {
            const std::string name = item.get<std::string>();

            if (!arguments.contains(name)) {
                return "Missing required argument: " + name;
            }
        }
    }

    const bool allowAdditionalProperties =
        schema.value("additionalProperties", true);

    for (auto argument = arguments.begin();
         argument != arguments.end();
         ++argument) {

        const std::string name = argument.key();

        if (!properties.contains(name)) {
            if (!allowAdditionalProperties) {
                return "Unexpected argument: " + name;
            }

            continue;
        }

        const Json& propertySchema = properties[name];

        if (propertySchema.contains("type")) {
            const std::string expectedType =
                propertySchema["type"].get<std::string>();

            if (!matchesType(argument.value(), expectedType)) {
                return "Argument '" + name +
                       "' must have type '" + expectedType + "'.";
            }
        }
    }

    return "";
}

} // namespace

void ToolRegistry::registerTool(
    ToolDefinition definition,
    ToolHandler handler
) {
    if (!isValidToolName(definition.name)) {
        throw std::invalid_argument(
            "Tool names must contain only letters, numbers, "
            "and underscores."
        );
    }

    if (definition.description.empty()) {
        throw std::invalid_argument(
            "A tool description is required."
        );
    }

    const std::string schemaError =
        validateSchemaDefinition(definition.parametersSchema);

    if (!schemaError.empty()) {
        throw std::invalid_argument(schemaError);
    }

    if (!handler) {
        throw std::invalid_argument(
            "A tool must have a valid handler."
        );
    }

    const std::string name = definition.name;

    const auto result = tools_.emplace(
        name,
        RegisteredTool{
            std::move(definition),
            std::move(handler)
        }
    );

    if (!result.second) {
        throw std::invalid_argument(
            "A tool with this name is already registered: " + name
        );
    }
}

bool ToolRegistry::contains(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

std::vector<ToolDefinition> ToolRegistry::definitions() const {
    std::vector<ToolDefinition> result;
    result.reserve(tools_.size());

    for (const auto& entry : tools_) {
        result.push_back(entry.second.definition);
    }

    return result;
}

ToolResult ToolRegistry::execute(
    const std::string& name,
    const Json& arguments
) const {
    const auto iterator = tools_.find(name);

    if (iterator == tools_.end()) {
        return {
            false,
            "Tool is not registered: " + name
        };
    }

    const std::string argumentError = validateArguments(
        iterator->second.definition.parametersSchema,
        arguments
    );

    if (!argumentError.empty()) {
        return {
            false,
            argumentError
        };
    }

    try {
        return iterator->second.handler(arguments);
    }
    catch (const std::exception& error) {
        return {
            false,
            std::string("Tool execution failed: ") + error.what()
        };
    }
    catch (...) {
        return {
            false,
            "Tool execution failed with an unknown error."
        };
    }
}

} // namespace numerick
