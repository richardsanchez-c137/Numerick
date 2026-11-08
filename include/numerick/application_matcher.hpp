#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace numerick {

// Returns matching candidate indices.
// Exact matches take priority over friendly-name prefix matches.
std::vector<std::size_t> matchApplicationNames(
    const std::wstring& requestedName,
    const std::vector<std::wstring>& candidates
);

} // namespace numerick