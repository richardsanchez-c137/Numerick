
#pragma once

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

class ToolRegistry {
public:
    void registerTool(
        ToolDefinition definition,
        ToolHandler handler
    );

    bool contains(const std::string& name) const;

    std::vector<ToolDefinition> definitions() const;

    LegacyToolResult execute(
        const std::string& name,
        const Json& arguments
    ) const;

private:
    struct RegisteredTool {
        ToolDefinition definition;
        ToolHandler handler;
    };

    std::map<std::string, RegisteredTool> tools_;
};

} // namespace numerick
