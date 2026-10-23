
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#include "numerick/ai_client.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace {

using json = nlohmann::json;

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

} // namespace

namespace numerick {

AIClient::AIClient(std::string model)
    : model_(std::move(model)) {}

std::string AIClient::ask(const std::string& prompt) const {
    if (prompt.empty()) {
        throw std::invalid_argument("The prompt cannot be empty.");
    }

    
        const json requestBody = {
            {"model", model_},
            {"messages", json::array({
                {
                    {"role", "system"},
                    {"content",
                        "You are NUMERICK, a personal AI assistant. "
                        "Answer the user's actual question directly. "
                        "For ordinary questions, prefer a concise answer "
                        "of 2-5 sentences. For simple explanations, aim "
                        "for fewer than 120 words unless more detail is "
                        "requested. Use clear language and formatting. "
                        "Never claim to have performed an action unless "
                        "it was actually performed and verified."
                    }
                },
                {
                    {"role", "user"},
                    {"content", prompt}
                }
            })},
            {"stream", false},
            {"think", false},
            {"keep_alive", "5m"},
            {"options", {
                {"num_predict", 512},
                {"temperature", 0.4}
            }}
        };


    const std::string payload = requestBody.dump();

    WinHttpHandle session{
        WinHttpOpen(
            L"NUMERICK/0.2.0",
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        )
    };

    if (!session.value) {
        throw winHttpError("Opening HTTP session");
    }

    WinHttpSetTimeouts(
        session.value,
        5000,    // DNS resolution
        5000,    // Connection
        15000,   // Send
        120000   // Receive
    );

    WinHttpHandle connection{
        WinHttpConnect(
            session.value,
            L"127.0.0.1",
            11434,
            0
        )
    };

    if (!connection.value) {
        throw winHttpError("Connecting to Ollama");
    }

    WinHttpHandle request{
        WinHttpOpenRequest(
            connection.value,
            L"POST",
            L"/api/chat",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            0
        )
    };

    if (!request.value) {
        throw winHttpError("Creating HTTP request");
    }

    const wchar_t* headers =
        L"Content-Type: application/json\r\n";

    if (!WinHttpSendRequest(
            request.value,
            headers,
            static_cast<DWORD>(-1),
            const_cast<char*>(payload.data()),
            static_cast<DWORD>(payload.size()),
            static_cast<DWORD>(payload.size()),
            0
        )) {
        throw winHttpError("Sending request to Ollama");
    }

    if (!WinHttpReceiveResponse(request.value, nullptr)) {
        throw winHttpError("Receiving Ollama response");
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);

    if (!WinHttpQueryHeaders(
            request.value,
            WINHTTP_QUERY_STATUS_CODE |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX
        )) {
        throw winHttpError("Reading HTTP status");
    }

    std::string responseBody;

    while (true) {
        DWORD available = 0;

        if (!WinHttpQueryDataAvailable(
                request.value, &available)) {
            throw winHttpError("Checking response data");
        }

        if (available == 0) {
            break;
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

        responseBody.append(buffer.data(), downloaded);
    }

    if (statusCode < 200 || statusCode >= 300) {
        throw std::runtime_error(
            "Ollama returned HTTP status " +
            std::to_string(statusCode) + "."
        );
    }

    const json response = json::parse(responseBody);

    if (!response.contains("message") ||
        !response["message"].is_object() ||
        !response["message"].contains("content") ||
        !response["message"]["content"].is_string()) {
        throw std::runtime_error(
            "Ollama returned an unexpected response format."
        );
    }

    
    const std::string content =
        response["message"]["content"].get<std::string>();

    if (response.value("done_reason", std::string{}) == "length") {
        return content +
            "\n\n[NUMERICK: The response may be incomplete "
            "because the output limit was reached.]";
    }

    return content;

}

} // namespace numerick
