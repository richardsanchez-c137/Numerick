
#pragma once

#include <string>

namespace numerick {

class AIClient {
public:
    explicit AIClient(std::string model = "qwen3.5:4b");

    std::string ask(const std::string& prompt) const;

private:
    std::string model_;
};

} // namespace numerick
