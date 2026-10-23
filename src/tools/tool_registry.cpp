
#include "numerick/tool_registry.hpp"

#include <cctype>
#include <exception>
#include <stdexcept>
#include <utility>

namespace numerick {
namespace {

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

    if (!definition.parametersSchema.is_object() ||
        definition.parametersSchema.value(
            "type", std::string{}
        ) != "object") {
        throw std::invalid_argument(
            "A tool must declare an object parameter schema."
        );
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

    if (!arguments.is_object()) {
        return {
            false,
            "Tool arguments must be a JSON object."
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
