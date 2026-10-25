#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace numerick {

// Bounds both the HTTP response accumulated in memory and the serialized
// request sent to the local Ollama service.
inline constexpr std::size_t kMaxOllamaPayloadBytes = 4U * 1024U * 1024U;

enum class OllamaFailureKind {
    Connection,
    Timeout,
    Transport,
    HttpStatus,
    ModelNotFound,
    MalformedJson,
    InvalidResponse,
    ResponseTooLarge,
    RequestTooLarge
};

class OllamaRequestError final : public std::runtime_error {
public:
    OllamaRequestError(
        OllamaFailureKind kind,
        const std::string& message,
        unsigned int statusCode = 0
    );

    OllamaFailureKind kind() const noexcept;
    unsigned int statusCode() const noexcept;

private:
    OllamaFailureKind kind_;
    unsigned int statusCode_;
};

// Appends a chunk only if the resulting body remains within maxBytes.
// The body is unchanged when the limit would be exceeded.
void appendOllamaResponseChunk(
    std::string& body,
    std::string_view chunk,
    std::size_t maxBytes = kMaxOllamaPayloadBytes
);

// Parses and validates Ollama's /api/chat response independently from WinHTTP,
// allowing malformed and oversized responses to be tested without a server.
nlohmann::json parseOllamaChatResponse(
    unsigned int statusCode,
    const std::string& body,
    bool toolsAllowed
);

} // namespace numerick
