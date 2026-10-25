#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace numerick {

struct OllamaConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 11434;
    std::string model = "qwen3.5:4b";
    std::uint32_t responseTimeoutMs = 120000;

    std::string endpoint() const;
};

struct OllamaHealthResult {
    bool serverResponded = false;
    bool apiValid = false;
    bool modelAvailable = false;
    std::string message;
};

// Parse a small, explicit environment map. Missing keys use safe defaults.
// Throws std::invalid_argument for invalid values.
OllamaConfig parseOllamaConfig(
    const std::map<std::string, std::string>& values
);

OllamaConfig loadOllamaConfig();
void validateOllamaConfig(const OllamaConfig& config);

// Evaluate the response from GET /api/tags independently of the network layer,
// making the doctor's interpretation testable without a running Ollama server.
OllamaHealthResult evaluateOllamaTagsResponse(
    unsigned int statusCode,
    const std::string& responseBody,
    const std::string& configuredModel
);

} // namespace numerick
