#include "numerick/application_matcher.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

namespace numerick {
namespace {

bool equalsIgnoreCase(
    const std::wstring& first,
    const std::wstring& second
) {
    return CompareStringOrdinal(
        first.c_str(), -1,
        second.c_str(), -1,
        TRUE
    ) == CSTR_EQUAL;
}

bool startsWithIgnoreCase(
    const std::wstring& value,
    const std::wstring& prefix
) {
    if (value.size() < prefix.size()) {
        return false;
    }

    return CompareStringOrdinal(
        value.c_str(),
        static_cast<int>(prefix.size()),
        prefix.c_str(),
        static_cast<int>(prefix.size()),
        TRUE
    ) == CSTR_EQUAL;
}

bool hasFriendlyNameSeparator(
    const std::wstring& value,
    std::size_t position
) {
    if (position >= value.size()) {
        return false;
    }

    const wchar_t character = value[position];

    return character == L'-' ||
           character == L'_' ||
           character == L' ';
}

} // namespace

std::vector<std::size_t> matchApplicationNames(
    const std::wstring& requestedName,
    const std::vector<std::wstring>& candidates
) {
    std::vector<std::size_t> exactMatches;
    std::vector<std::size_t> friendlyMatches;

    if (requestedName.empty()) {
        return {};
    }

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const std::wstring& candidate = candidates[index];

        if (equalsIgnoreCase(candidate, requestedName)) {
            exactMatches.push_back(index);
            continue;
        }

        if (candidate.size() > requestedName.size() &&
            startsWithIgnoreCase(candidate, requestedName) &&
            hasFriendlyNameSeparator(candidate, requestedName.size())) {
            friendlyMatches.push_back(index);
        }
    }

    return exactMatches.empty()
        ? friendlyMatches
        : exactMatches;
}

} // namespace numerick