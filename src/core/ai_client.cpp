#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#include "numerick/ai_client.hpp"
#include "numerick/ollama_config.hpp"

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

std::runtime_error winHttpError(const std::string& stage) {
    return std::runtime_error(
        stage + " failed. Windows error: " +
        std::to_string(GetLastError())
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

    WinHttpHandle session{
        WinHttpOpen(
            L"NUMERICK/1.1.0",
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

    constexpr std::size_t kMaxResponseBytes = 4U * 1024U * 1024U;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) {
            throw winHttpError("Checking response data");
        }
        if (available == 0) {
            break;
        }

        if (response.body.size() + static_cast<std::size_t>(available) >
            kMaxResponseBytes) {
            throw std::runtime_error(
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
        response.body.append(buffer.data(), downloaded);
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

    if (response.statusCode < 200 || response.statusCode >= 300) {
        throw std::runtime_error(
            "Ollama returned HTTP status " +
            std::to_string(response.statusCode) + "."
        );
    }

    const Json parsed = Json::parse(response.body);
    if (!parsed.is_object() ||
        !parsed.contains("message") ||
        !parsed["message"].is_object()) {
        throw std::runtime_error(
            "Ollama returned an unexpected response format."
        );
    }

    return parsed;
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
