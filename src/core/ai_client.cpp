#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#include "numerick/ai_client.hpp"
#include "numerick/ollama_config.hpp"
#include "numerick/ollama_response.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace {

using Json = nlohmann::json;

struct WinHttpHandle {
    HINTERNET value = nullptr;

    explicit WinHttpHandle(HINTERNET handle = nullptr)
        : value(handle) {}

    ~WinHttpHandle() {
        if (value != nullptr) {
            WinHttpCloseHandle(value);
        }
    }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
};

numerick::OllamaRequestError winHttpError(const std::string& stage) {
    const DWORD errorCode = GetLastError();
    numerick::OllamaFailureKind kind = numerick::OllamaFailureKind::Transport;
    std::string prefix = "Ollama HTTP transport failed";

    if (errorCode == ERROR_WINHTTP_TIMEOUT) {
        kind = numerick::OllamaFailureKind::Timeout;
        prefix = "Ollama request timed out";
    }
    else if (errorCode == ERROR_WINHTTP_CANNOT_CONNECT ||
             errorCode == ERROR_WINHTTP_NAME_NOT_RESOLVED ||
             errorCode == ERROR_WINHTTP_CONNECTION_ERROR) {
        kind = numerick::OllamaFailureKind::Connection;
        prefix = "Could not connect to Ollama";
    }

    return numerick::OllamaRequestError(
        kind,
        prefix + " while " + stage + ". Windows error: " +
            std::to_string(errorCode)
    );
}

struct HttpResponse {
    unsigned int statusCode = 0;
    std::string body;
};

HttpResponse performOllamaRequest(
    const numerick::OllamaConfig& config,
    const wchar_t* method,
    const wchar_t* path,
    const std::string& payload
) {
    const std::wstring wideHost(config.host.begin(), config.host.end());

    if (payload.size() > numerick::kMaxOllamaPayloadBytes) {
        throw numerick::OllamaRequestError(
            numerick::OllamaFailureKind::RequestTooLarge,
            "Ollama request exceeded the 4 MiB safety limit."
        );
    }

    WinHttpHandle session{
        WinHttpOpen(
            L"NUMERICK/1.2.0",
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        )
    };

    if (!session.value) {
        throw winHttpError("Opening HTTP session");
    }

    if (!WinHttpSetTimeouts(
            session.value, 5000, 5000, 15000,
            static_cast<int>(config.responseTimeoutMs))) {
        throw winHttpError("Setting HTTP timeouts");
    }

    WinHttpHandle connection{
        WinHttpConnect(
            session.value,
            wideHost.c_str(),
            static_cast<INTERNET_PORT>(config.port),
            0
        )
    };

    if (!connection.value) {
        throw winHttpError("Connecting to Ollama at " + config.endpoint());
    }

    WinHttpHandle request{
        WinHttpOpenRequest(
            connection.value,
            method,
            path,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            0
        )
    };

    if (!request.value) {
        throw winHttpError("Creating HTTP request");
    }

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    LPVOID requestData = payload.empty()
        ? WINHTTP_NO_REQUEST_DATA
        : const_cast<char*>(payload.data());

    if (!WinHttpSendRequest(
            request.value,
            headers,
            static_cast<DWORD>(-1),
            requestData,
            static_cast<DWORD>(payload.size()),
            static_cast<DWORD>(payload.size()),
            0
        )) {
        throw winHttpError("Sending request to Ollama");
    }

    if (!WinHttpReceiveResponse(request.value, nullptr)) {
        throw winHttpError("Receiving response from Ollama");
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
            request.value,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX
        )) {
        throw winHttpError("Reading HTTP status");
    }

    HttpResponse response;
    response.statusCode = statusCode;

    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) {
            throw winHttpError("Checking response data");
        }
        if (available == 0) {
            break;
        }

        if (response.body.size() > numerick::kMaxOllamaPayloadBytes ||
            static_cast<std::size_t>(available) >
                numerick::kMaxOllamaPayloadBytes - response.body.size()) {
            throw numerick::OllamaRequestError(
                numerick::OllamaFailureKind::ResponseTooLarge,
                "Ollama response exceeded the 4 MiB safety limit."
            );
        }

        std::string buffer(available, '\0');
        DWORD downloaded = 0;
        if (!WinHttpReadData(
                request.value,
                buffer.data(),
                available,
                &downloaded
            )) {
            throw winHttpError("Reading response data");
        }
        if (downloaded == 0) {
            throw numerick::OllamaRequestError(
                numerick::OllamaFailureKind::Transport,
                "Reading the Ollama response made no progress."
            );
        }
        numerick::appendOllamaResponseChunk(
            response.body, std::string_view(buffer.data(), downloaded)
        );
    }

    return response;
}

} // namespace

namespace numerick {

AIClient::AIClient()
    : config_(loadOllamaConfig()) {}

AIClient::AIClient(OllamaConfig config)
    : config_(std::move(config)) {
    validateOllamaConfig(config_);
}

AIClient::AIClient(std::string model)
    : config_(loadOllamaConfig()) {
    if (model.empty()) {
        throw std::invalid_argument("The Ollama model name cannot be empty.");
    }
    config_.model = std::move(model);
    validateOllamaConfig(config_);
}

const OllamaConfig& AIClient::config() const noexcept {
    return config_;
}

OllamaHealthResult AIClient::diagnose() const {
    try {
        const HttpResponse response = performOllamaRequest(
            config_, L"GET", L"/api/tags", {}
        );
        return evaluateOllamaTagsResponse(
            response.statusCode, response.body, config_.model
        );
    }
    catch (const OllamaRequestError& error) {
        std::string message;
        switch (error.kind()) {
        case OllamaFailureKind::Connection:
            message = "Ollama is unreachable at http://" + config_.endpoint() +
                ". Start Ollama and verify the configured host and port.";
            break;
        case OllamaFailureKind::Timeout:
            message = "Ollama did not respond before the configured timeout.";
            break;
        case OllamaFailureKind::ModelNotFound:
            message = "The configured Ollama model is missing. Run `ollama pull " +
                config_.model + "`.";
            break;
        case OllamaFailureKind::HttpStatus:
            message = "Ollama returned HTTP status " +
                std::to_string(error.statusCode()) + " while checking its API.";
            break;
        case OllamaFailureKind::MalformedJson:
            message = "Ollama returned malformed JSON while checking its API.";
            break;
        case OllamaFailureKind::InvalidResponse:
            message = "Ollama returned an unexpected API response.";
            break;
        case OllamaFailureKind::ResponseTooLarge:
            message = "Ollama's response exceeded the configured safety limit.";
            break;
        case OllamaFailureKind::RequestTooLarge:
            message = "The request exceeded the configured safety limit.";
            break;
        case OllamaFailureKind::Transport:
            message = "Communication with Ollama failed.";
            break;
        }
        return {false, false, false, message + " Details: " + error.what()};
    }
    catch (const Json::parse_error& error) {
        return {
            false, false, false,
            "Ollama responded, but /api/tags returned malformed JSON. Details: " +
                std::string(error.what())
        };
    }
    catch (const std::exception& error) {
        return {
            false,
            false,
            false,
            "Could not contact Ollama at http://" + config_.endpoint() +
                ". Check that Ollama is running and the host/port are correct. Details: " +
                error.what()
        };
    }
}

Json AIClient::chat(
    const Json& messages,
    const Json& tools
) const {
    if (!messages.is_array() || messages.empty()) {
        throw std::invalid_argument(
            "Chat messages must be a non-empty JSON array."
        );
    }

    if (!tools.is_array()) {
        throw std::invalid_argument("Tools must be a JSON array.");
    }

    Json requestBody = {
        {"model", config_.model},
        {"messages", messages},
        {"stream", false},
        {"think", false},
        {"keep_alive", "5m"},
        {"options", {
            {"num_predict", 512},
            {"temperature", 0.4}
        }}
    };

    if (!tools.empty()) {
        requestBody["tools"] = tools;
    }

    const HttpResponse response = performOllamaRequest(
        config_, L"POST", L"/api/chat", requestBody.dump()
    );

    return parseOllamaChatResponse(
        response.statusCode, response.body, !tools.empty()
    );
}

std::string AIClient::ask(const std::string& prompt) const {
    if (prompt.empty()) {
        throw std::invalid_argument("The prompt cannot be empty.");
    }

    const Json messages = Json::array({
        {
            {"role", "system"},
            {"content",
                "You are NUMERICK, a personal AI assistant. "
                "Answer clearly and directly. Be concise for "
                "ordinary questions. Never claim to have "
                "performed an action unless it was verified."
            }
        },
        {
            {"role", "user"},
            {"content", prompt}
        }
    });

    const Json response = chat(messages);
    const Json& message = response["message"];

    if (!message.contains("content") ||
        !message["content"].is_string()) {
        throw std::runtime_error(
            "Ollama response did not contain text content."
        );
    }

    const std::string content = message["content"].get<std::string>();
    if (response.value("done_reason", std::string{}) == "length") {
        return content +
            "\n\n[NUMERICK: The response may be incomplete "
            "because the output limit was reached.]";
    }
    return content;
}

} // namespace numerick
