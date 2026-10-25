#pragma once

#include "numerick/chat_client.hpp"
#include "numerick/ollama_config.hpp"

#include <string>

namespace numerick {

class AIClient : public ChatClient {
public:
    AIClient();
    explicit AIClient(OllamaConfig config);
    // Compatibility overload for callers that want to override only the model.
    explicit AIClient(std::string model);

    std::string ask(const std::string& prompt) const;

    nlohmann::json chat(
        const nlohmann::json& messages,
        const nlohmann::json& tools =
            nlohmann::json::array()
    ) const override;

    const OllamaConfig& config() const noexcept;
    OllamaHealthResult diagnose() const;

private:
    OllamaConfig config_;
};

} // namespace numerick
