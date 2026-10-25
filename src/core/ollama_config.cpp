#include "numerick/ollama_config.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace numerick {
namespace {

using Json = nlohmann::json;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::uint32_t parseUnsigned(
    const std::string& value,
    const char* variableName
) {
    if (value.empty()) {
        throw std::invalid_argument(
            std::string(variableName) + " must be a decimal number."
        );
    }

    unsigned long long parsed = 0;
    const char* first = value.data();
    const char* last = first + value.size();
    const auto result = std::from_chars(first, last, parsed);

    if (result.ec != std::errc{} || result.ptr != last ||
        parsed > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument(
            std::string(variableName) + " must be a valid decimal number."
        );
    }

    return static_cast<std::uint32_t>(parsed);
}

std::string valueOrDefault(
    const std::map<std::string, std::string>& values,
    const char* key,
    const char* defaultValue
) {
    const auto found = values.find(key);
    return found == values.end() ? defaultValue : found->second;
}

} // namespace

std::string OllamaConfig::endpoint() const {
    return host + ":" + std::to_string(port);
}

void validateOllamaConfig(const OllamaConfig& config) {
    if (config.host.empty() || config.host.size() > 253) {
        throw std::invalid_argument(
            "NUMERICK_OLLAMA_HOST must be a hostname or IPv4 address "
            "between 1 and 253 characters."
        );
    }

    for (unsigned char character : config.host) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '.' || character == '-' || character == '_';
        if (!allowed) {
            throw std::invalid_argument(
                "NUMERICK_OLLAMA_HOST accepts only hostname/IPv4 characters "
                "(letters, digits, dot, hyphen, underscore); do not include a scheme or path."
            );
        }
    }

    if (config.port == 0) {
        throw std::invalid_argument(
            "NUMERICK_OLLAMA_PORT must be between 1 and 65535."
        );
    }

    if (config.model.empty() || config.model.size() > 200) {
        throw std::invalid_argument(
            "NUMERICK_OLLAMA_MODEL must contain between 1 and 200 characters."
        );
    }

    for (unsigned char character : config.model) {
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                "NUMERICK_OLLAMA_MODEL must not contain control characters."
            );
        }
    }

    if (config.responseTimeoutMs < 1000U ||
        config.responseTimeoutMs > 300000U) {
        throw std::invalid_argument(
            "NUMERICK_OLLAMA_TIMEOUT_MS must be between 1000 and 300000."
        );
    }
}

OllamaConfig parseOllamaConfig(
    const std::map<std::string, std::string>& values
) {
    OllamaConfig config;
    config.host = trim(valueOrDefault(
        values, "NUMERICK_OLLAMA_HOST", "127.0.0.1"
    ));
    config.model = trim(valueOrDefault(
        values, "NUMERICK_OLLAMA_MODEL", "qwen3.5:4b"
    ));

    const std::uint32_t parsedPort = parseUnsigned(
        valueOrDefault(values, "NUMERICK_OLLAMA_PORT", "11434"),
        "NUMERICK_OLLAMA_PORT"
    );
    if (parsedPort < 1U || parsedPort > 65535U) {
        throw std::invalid_argument(
            "NUMERICK_OLLAMA_PORT must be between 1 and 65535."
        );
    }
    config.port = static_cast<std::uint16_t>(parsedPort);

    config.responseTimeoutMs = parseUnsigned(
        valueOrDefault(values, "NUMERICK_OLLAMA_TIMEOUT_MS", "120000"),
        "NUMERICK_OLLAMA_TIMEOUT_MS"
    );

    validateOllamaConfig(config);
    return config;
}

OllamaConfig loadOllamaConfig() {
    std::map<std::string, std::string> values;
    const char* variableNames[] = {
        "NUMERICK_OLLAMA_HOST",
        "NUMERICK_OLLAMA_PORT",
        "NUMERICK_OLLAMA_MODEL",
        "NUMERICK_OLLAMA_TIMEOUT_MS"
    };

    for (const char* name : variableNames) {
        const char* value = std::getenv(name);
        if (value != nullptr) {
            values.emplace(name, value);
        }
    }

    return parseOllamaConfig(values);
}

OllamaHealthResult evaluateOllamaTagsResponse(
    unsigned int statusCode,
    const std::string& responseBody,
    const std::string& configuredModel
) {
    OllamaHealthResult result;
    result.serverResponded = statusCode >= 100U && statusCode <= 599U;

    if (statusCode < 200U || statusCode >= 300U) {
        result.message = "Ollama responded with HTTP status " +
            std::to_string(statusCode) + " for /api/tags.";
        return result;
    }

    Json response;
    try {
        response = Json::parse(responseBody);
    }
    catch (const std::exception&) {
        result.message =
            "Ollama responded, but /api/tags returned invalid JSON.";
        return result;
    }

    if (!response.is_object() ||
        !response.contains("models") ||
        !response["models"].is_array()) {
        result.message =
            "Ollama responded, but /api/tags did not contain a models array.";
        return result;
    }

    result.apiValid = true;
    for (const Json& model : response["models"]) {
        if (model.is_object() &&
            model.contains("name") &&
            model["name"].is_string() &&
            model["name"].get<std::string>() == configuredModel) {
            result.modelAvailable = true;
            break;
        }
    }

    if (result.modelAvailable) {
        result.message = "Ollama is reachable and configured model '" +
            configuredModel + "' is installed.";
    }
    else {
        result.message = "Ollama is reachable, but configured model '" +
            configuredModel + "' was not found. Run: ollama pull " +
            configuredModel;
    }

    return result;
}

} // namespace numerick
