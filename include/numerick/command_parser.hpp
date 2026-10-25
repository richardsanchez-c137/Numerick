#pragma once

#include <string>

namespace numerick {

enum class CommandType {
    Help,
    Greeting,
    Status,
    Time,
    Ask,
    Agent,
    Remember,
    Memories,
    Forget,
    Clear,
    Exit,
    Unknown
};

struct ParsedCommand {
    CommandType type = CommandType::Unknown;
    std::string argument;
};

ParsedCommand parseCommand(const std::string& input);

} // namespace numerick
