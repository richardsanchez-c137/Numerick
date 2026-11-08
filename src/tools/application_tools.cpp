#include "numerick/application_tools.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>
#include "numerick/application_matcher.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace numerick {
namespace {

std::wstring toWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()),
        nullptr, 0
    );

    if (length <= 0) {
        return {};
    }

    std::wstring result(length, L'\0');

    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()),
            result.data(), length
        ) != length) {
        return {};
    }

    return result;
}

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

std::vector<std::filesystem::path> getStartMenuRoots() {
    std::vector<std::filesystem::path> roots;

    const KNOWNFOLDERID folders[] = {
        FOLDERID_Programs,
        FOLDERID_CommonPrograms
    };

    for (const auto& folder : folders) {
        PWSTR rawPath = nullptr;

        const HRESULT result = SHGetKnownFolderPath(
            folder, 0, nullptr, &rawPath
        );

        if (FAILED(result) || rawPath == nullptr) {
            continue;
        }

        std::filesystem::path path(rawPath);
        CoTaskMemFree(rawPath);

        if (std::filesystem::is_directory(path)) {
            roots.push_back(std::move(path));
        }
    }

    return roots;
}

std::vector<std::filesystem::path> findShortcuts(
    const std::wstring& applicationName
) {
    std::vector<std::filesystem::path> shortcuts;
    std::vector<std::wstring> shortcutNames;

    // Discover all Start Menu shortcuts.
    for (const auto& root : getStartMenuRoots()) {
        std::error_code error;

        std::filesystem::recursive_directory_iterator iterator(
            root,
            std::filesystem::directory_options::skip_permission_denied,
            error
        );

        const std::filesystem::recursive_directory_iterator end;

        while (iterator != end) {
            if (error) {
                error.clear();
                iterator.increment(error);
                continue;
            }

            const auto& entry = *iterator;
            std::error_code fileError;

            if (entry.is_regular_file(fileError) &&
                equalsIgnoreCase(
                    entry.path().extension().wstring(),
                    L".lnk"
                )) {
                shortcuts.push_back(entry.path());
                shortcutNames.push_back(entry.path().stem().wstring());
            }

            iterator.increment(error);
        }
    }

    // Use the independently tested application matcher.
    const auto matchingIndices = matchApplicationNames(
        applicationName,
        shortcutNames
    );

    std::vector<std::filesystem::path> matches;
    matches.reserve(matchingIndices.size());

    for (const std::size_t index : matchingIndices) {
        if (index < shortcuts.size()) {
            matches.push_back(shortcuts[index]);
        }
    }

    return matches;
}

std::optional<std::filesystem::path> findExecutable(
    const std::wstring& applicationName
) {
    if (applicationName.empty()) {
        return std::nullopt;
    }

    // Only accept executable names, never arbitrary paths or commands.
    for (const wchar_t character : applicationName) {
        const bool valid =
            (character >= L'a' && character <= L'z') ||
            (character >= L'A' && character <= L'Z') ||
            (character >= L'0' && character <= L'9') ||
            character == L' ' ||
            character == L'-' ||
            character == L'_' ||
            character == L'.';

        if (!valid) {
            return std::nullopt;
        }
    }

    std::wstring executableName = applicationName;

    if (executableName.find_last_of(L'.') == std::wstring::npos) {
        executableName += L".exe";
    }
    else if (!equalsIgnoreCase(
                 std::filesystem::path(executableName)
                     .extension().wstring(),
                 L".exe")) {
        return std::nullopt;
    }

    std::wstring searchPath;

    auto addDirectory = [&](const std::wstring& directory) {
        if (directory.empty()) {
            return;
        }

        if (!searchPath.empty()) {
            searchPath += L';';
        }

        searchPath += directory;
    };

    // Search Windows System32 first.
    std::vector<wchar_t> systemBuffer(32768);

    const UINT systemLength = GetSystemDirectoryW(
        systemBuffer.data(),
        static_cast<UINT>(systemBuffer.size())
    );

    if (systemLength > 0 &&
        systemLength < systemBuffer.size()) {
        addDirectory(
            std::wstring(systemBuffer.data(), systemLength)
        );
    }

    // Read PATH, excluding relative and empty entries.
    DWORD required = GetEnvironmentVariableW(
        L"PATH", nullptr, 0
    );

    if (required > 1) {
        std::vector<wchar_t> pathBuffer(required);

        const DWORD written = GetEnvironmentVariableW(
            L"PATH",
            pathBuffer.data(),
            required
        );

        if (written > 0 && written < required) {
            const std::wstring pathValue(
                pathBuffer.data(), written
            );

            std::size_t start = 0;

            while (start <= pathValue.size()) {
                const std::size_t end =
                    pathValue.find(L';', start);

                std::wstring directory = pathValue.substr(
                    start,
                    end == std::wstring::npos
                        ? std::wstring::npos
                        : end - start
                );

                // Remove optional surrounding quotes.
                if (directory.size() >= 2 &&
                    directory.front() == L'"' &&
                    directory.back() == L'"') {
                    directory =
                        directory.substr(1, directory.size() - 2);
                }

                std::error_code error;

                const std::filesystem::path directoryPath(
                    directory
                );

                if (!directory.empty() &&
                    directoryPath.is_absolute() &&
                    std::filesystem::is_directory(
                        directoryPath, error
                    ) &&
                    !error) {
                    addDirectory(directory);
                }

                if (end == std::wstring::npos) {
                    break;
                }

                start = end + 1;
            }
        }
    }

    if (searchPath.empty()) {
        return std::nullopt;
    }

    // Resolve an actual executable using the explicit search path.
    std::vector<wchar_t> resultBuffer(32768);

    const DWORD result = SearchPathW(
        searchPath.c_str(),
        executableName.c_str(),
        nullptr,
        static_cast<DWORD>(resultBuffer.size()),
        resultBuffer.data(),
        nullptr
    );

    if (result == 0 || result >= resultBuffer.size()) {
        return std::nullopt;
    }

    std::filesystem::path executable(
        resultBuffer.data()
    );

    std::error_code error;

    if (!std::filesystem::is_regular_file(executable, error) ||
        error) {
        return std::nullopt;
    }

    return executable;
}

bool launchShellTarget(
    const std::wstring& target,
    const std::wstring& parameters,
    std::string& message
) {
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        target.c_str(),
        parameters.empty() ? nullptr : parameters.c_str(),
        nullptr,
        SW_SHOWNORMAL
    );

    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        message = "Windows could not launch the requested application.";
        return false;
    }

    return true;
}

LegacyToolResult openApplication(const Json& arguments) {
    if (!arguments.is_object() ||
        arguments.size() != 1 ||
        !arguments.contains("application_name") ||
        !arguments["application_name"].is_string()) {
        return {
            false,
            "Expected exactly one string argument: application_name."
        };
    }

    const std::string requested =
        arguments["application_name"].get<std::string>();

    if (requested.empty() || requested.size() > 128) {
        return {
            false,
            "Application name must contain between 1 and 128 bytes."
        };
    }

    const std::wstring applicationName = toWide(requested);

    if (applicationName.empty()) {
        return {
            false,
            "Application name contains invalid UTF-8."
        };
    }

    std::string launchError;

    // Windows Camera is a packaged Windows application.
    if (equalsIgnoreCase(applicationName, L"camera")) {
        if (launchShellTarget(
                L"explorer.exe",
                L"shell:AppsFolder\\Microsoft.WindowsCamera_8wekyb3d8bbwe!App",
                launchError)) {
            return {
                true,
                "Windows Camera launch request submitted."
            };
        }
    }

    // Spotify may register its own application URI protocol.
    if (equalsIgnoreCase(applicationName, L"spotify")) {
        if (launchShellTarget(
                L"spotify:", L"", launchError)) {
            return {
                true,
                "Spotify launch request submitted."
            };
        }
    }

    // First search Start Menu shortcuts.
    const auto matches = findShortcuts(applicationName);

    if (matches.size() > 1) {
        return {
            false,
            "Multiple Start Menu shortcuts match '" +
                requested + "'. Please use a more specific name."
        };
    }

    if (matches.size() == 1) {
        if (!launchShellTarget(
                matches.front().wstring(), L"", launchError)) {
            return {
                false,
                launchError
            };
        }

        return {
            true,
            "Launch request submitted for " + requested + "."
        };
    }

    // Next search Windows executable directories.
    const auto executable = findExecutable(applicationName);

    if (!executable) {
        return {
            false,
            "Application not found in Start Menu shortcuts or "
            "Windows executable search paths: " + requested
        };
    }

    if (!launchShellTarget(
            executable->wstring(), L"", launchError)) {
        return {
            false,
            launchError
        };
    }

    return {
        true,
        "Launch request submitted for " + requested + "."
    };

} // namespace
} // namespace
void registerApplicationTools(ToolRegistry& registry) {
    registry.registerTool(
        {
            "open_application",
            "Opens an installed Windows application by its exact Start Menu "
            "name or an unambiguous friendly-name prefix. Never executes "
            "arbitrary commands.",
            {
                {"type", "object"},
                {
                    "properties",
                    {
                        {
                            "application_name",
                            {
                                {"type", "string"}
                            }
                        }
                    }
                },
                {"required", {"application_name"}},
                {"additionalProperties", false}
            }
        },
        openApplication
    );
}

} // namespace numerick
