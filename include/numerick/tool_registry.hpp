
#pragma once

#include "numerick/tool_result.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace numerick {

using Json = nlohmann::json;

struct ToolDefinition {
    std::string name;
    std::string description;
    Json parametersSchema;
};

struct LegacyToolResult {
    bool success = false;
    std::string output;
};

using ToolHandler =
    std::function<LegacyToolResult(const Json& arguments)>;
using StructuredToolHandler =
    std::function<ToolResult(const Json& arguments)>;

class ToolRegistry {
public:
    void registerTool(
        ToolDefinition definition,
        ToolHandler handler
    );

    void registerTool(
        ToolDefinition definition,
        StructuredToolHandler handler
    );

    bool contains(const std::string& name) const;

    std::vector<ToolDefinition> definitions() const;

    ToolResult execute(
        const std::string& name,
        const Json& arguments
    ) const;

    // Temporary compatibility adapter for unmigrated consumers.
    LegacyToolResult executeLegacy(
        const std::string& name,
        const Json& arguments
    ) const;

private:
    struct RegisteredTool {
        ToolDefinition definition;
        ToolHandler legacyHandler;
        StructuredToolHandler handler;
    };

    void registerToolImpl(
        ToolDefinition definition,
        ToolHandler legacyHandler,
        StructuredToolHandler handler
    );

    std::map<std::string, RegisteredTool> tools_;
};

} // namespace numerick
