
#pragma once

#include "numerick/chat_client.hpp"

#include <string>

namespace numerick {

class AIClient : public ChatClient {
public:
    explicit AIClient(std::string model = "qwen3.5:4b");

    std::string ask(const std::string& prompt) const;

    nlohmann::json chat(
        const nlohmann::json& messages,
        const nlohmann::json& tools =
            nlohmann::json::array()
    ) const override;

private:
    std::string model_;
};

} // namespace numerick
