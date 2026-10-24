
#pragma once

#include <nlohmann/json.hpp>

namespace numerick {

class ChatClient {
public:
    virtual ~ChatClient() = default;

    virtual nlohmann::json chat(
        const nlohmann::json& messages,
        const nlohmann::json& tools =
            nlohmann::json::array()
    ) const = 0;
};

} // namespace numerick
