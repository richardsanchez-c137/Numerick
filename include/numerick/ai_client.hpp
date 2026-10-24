#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace numerick {

class AIClient {
public:
    explicit AIClient(std::string model = "qwen3.5:4b");

    // Ask a normal question and return the final text.
    std::string ask(const std::string& prompt) const;

    // Send a chat request and return the complete Ollama response.
    nlohmann::json chat(
        const nlohmann::json& messages,
        const nlohmann::json& tools =
            nlohmann::json::array()
    ) const;

private:
    std::string model_;
};

} // namespace numerick
