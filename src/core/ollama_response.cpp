#include "numerick/ollama_response.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace numerick {
namespace {

using Json = nlohmann::json;

bool isBlank(const std::string& value) {
    return value.find_first_not_of(" \t\r\n") == std::string::npos;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string errorTextFromBody(const std::string& body) {
    try {
        const Json errorJson = Json::parse(body);
        if (errorJson.is_object() && errorJson.contains("error") &&
            errorJson["error"].is_string()) {
            std::string error = errorJson["error"].get<std::string>();
            constexpr std::size_t kMaxErrorText = 240;
            if (error.size() > kMaxErrorText) {
                error.resize(kMaxErrorText);
                error += "...";
            }
            return error;
        }
    }
    catch (const Json::exception&) {
        // An HTTP error may contain plain text or malformed JSON. Preserve the
        // HTTP status as the primary diagnostic instead of masking it.
    }
    return {};
}

bool looksLikeMissingModel(const std::string& error) {
    const std::string text = lowercase(error);
    return text.find("model") != std::string::npos &&
        (text.find("not found") != std::string::npos ||
         text.find("does not exist") != std::string::npos ||
         text.find("no such model") != std::string::npos);
}

[[noreturn]] void invalidResponse(const std::string& reason) {
    throw OllamaRequestError(
        OllamaFailureKind::InvalidResponse,
        "Ollama returned an invalid /api/chat response: " + reason
    );
}

} // namespace

OllamaRequestError::OllamaRequestError(
    OllamaFailureKind kind,
    const std::string& message,
    unsigned int statusCode
)
    : std::runtime_error(message), kind_(kind), statusCode_(statusCode) {}

OllamaFailureKind OllamaRequestError::kind() const noexcept {
    return kind_;
}

unsigned int OllamaRequestError::statusCode() const noexcept {
    return statusCode_;
}

void appendOllamaResponseChunk(
    std::string& body,
    std::string_view chunk,
    std::size_t maxBytes
) {
    if (body.size() > maxBytes || chunk.size() > maxBytes - body.size()) {
        throw OllamaRequestError(
            OllamaFailureKind::ResponseTooLarge,
            "Ollama response exceeded the " + std::to_string(maxBytes) +
                " byte safety limit."
        );
    }
    if (!chunk.empty()) {
        body.append(chunk.data(), chunk.size());
    }
}

Json parseOllamaChatResponse(
    unsigned int statusCode,
    const std::string& body,
    bool toolsAllowed
) {
    if (body.size() > kMaxOllamaPayloadBytes) {
        throw OllamaRequestError(
            OllamaFailureKind::ResponseTooLarge,
            "Ollama response exceeded the 4 MiB safety limit."
        );
    }

    if (statusCode < 200 || statusCode >= 300) {
        const std::string serverError = errorTextFromBody(body);
        if (looksLikeMissingModel(serverError)) {
            throw OllamaRequestError(
                OllamaFailureKind::ModelNotFound,
                "The configured Ollama model is not installed or could not be found. "
                "Run `ollama pull <model>` for the configured model. Server detail: " +
                    serverError,
                statusCode
            );
        }

        std::string message = "Ollama returned HTTP status " +
            std::to_string(statusCode) + ".";
        if (!serverError.empty()) {
            message += " Server detail: " + serverError;
        }
        throw OllamaRequestError(
            OllamaFailureKind::HttpStatus, message, statusCode
        );
    }

    Json response;
    try {
        response = Json::parse(body);
    }
    catch (const Json::parse_error&) {
        throw OllamaRequestError(
            OllamaFailureKind::MalformedJson,
            "Ollama returned malformed JSON from /api/chat."
        );
    }

    if (!response.is_object() || !response.contains("message") ||
        !response["message"].is_object()) {
        invalidResponse("the `message` field is missing or is not an object.");
    }

    const Json& message = response["message"];
    if (!message.contains("content") || !message["content"].is_string()) {
        invalidResponse("`message.content` is missing or is not a string.");
    }

    Json requestedCalls = Json::array();
    if (message.contains("tool_calls")) {
        if (!message["tool_calls"].is_array()) {
            invalidResponse("`message.tool_calls` is not an array.");
        }
        requestedCalls = message["tool_calls"];
    }

    if (!requestedCalls.empty() && !toolsAllowed) {
        invalidResponse("the model requested tools when none were supplied.");
    }

    for (const Json& call : requestedCalls) {
        if (!call.is_object() || !call.contains("function") ||
            !call["function"].is_object()) {
            invalidResponse("a tool call has no valid `function` object.");
        }
        const Json& function = call["function"];
        if (!function.contains("name") || !function["name"].is_string() ||
            isBlank(function["name"].get<std::string>()) ||
            !function.contains("arguments") ||
            !function["arguments"].is_object()) {
            invalidResponse(
                "a tool call needs a non-empty function name and object arguments."
            );
        }
    }

    const std::string content = message["content"].get<std::string>();
    if (requestedCalls.empty() && isBlank(content)) {
        invalidResponse("the assistant returned empty text and no tool calls.");
    }

    return response;
}

} // namespace numerick
