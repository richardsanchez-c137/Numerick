#include "numerick/command_parser.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace numerick {
namespace {

std::string trim(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

std::string lowercase(std::string input) {
    std::transform(
        input.begin(),
        input.end(),
        input.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return input;
}

} // namespace

ParsedCommand parseCommand(const std::string& input) {
    const std::string cleaned = trim(input);
    const std::string normalized = lowercase(cleaned);

    if (normalized.empty()) {
        return {CommandType::Unknown, ""};
    }

    if (normalized == "help" || normalized == "commands") {
        return {CommandType::Help, ""};
    }

    if (normalized == "hello" ||
        normalized == "hi" ||
        normalized == "hey") {
        return {CommandType::Greeting, ""};
    }

    if (normalized == "status") {
        return {CommandType::Status, ""};
    }

    if (normalized == "doctor") {
        return {CommandType::Doctor, ""};
    }

    if (normalized == "time") {
        return {CommandType::Time, ""};
    }

    if (normalized == "clear") {
        return {CommandType::Clear, ""};
    }

    if (normalized == "exit" || normalized == "quit") {
        return {CommandType::Exit, ""};
    }

    if (normalized == "ask") {
        return {CommandType::Ask, ""};
    }

    if (normalized.rfind("ask ", 0) == 0) {
        return {CommandType::Ask, trim(cleaned.substr(4))};
    }

    if (normalized == "agent") {
        return {CommandType::Agent, ""};
    }

    if (normalized.rfind("agent ", 0) == 0) {
        return {CommandType::Agent, trim(cleaned.substr(6))};
    }

    if (normalized == "remember") {
        return {CommandType::Remember, ""};
    }

    if (normalized.rfind("remember ", 0) == 0) {
        return {CommandType::Remember, trim(cleaned.substr(9))};
    }

    if (normalized == "memory" || normalized == "memories") {
        return {CommandType::Memories, ""};
    }

    if (normalized == "forget") {
        return {CommandType::Forget, ""};
    }

    if (normalized.rfind("forget ", 0) == 0) {
        return {CommandType::Forget, trim(cleaned.substr(7))};
    }

    return {CommandType::Unknown, cleaned};
}

} // namespace numerick
