#pragma once

#include "numerick/tool_registry.hpp"

#include <filesystem>

namespace numerick {

// Resolves NUMERICK_WORKSPACE_ROOT when set; otherwise uses the current
// working directory. The result is canonical and must be an existing folder.
std::filesystem::path configuredWorkspaceRoot();

// Registers read-only tools that can access only safe text files beneath root.
void registerProjectFileTools(
    ToolRegistry& registry,
    const std::filesystem::path& workspaceRoot
);

} // namespace numerick
